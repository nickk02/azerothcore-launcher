#include "pch.h"
#include "HomePage.h"
#if __has_include("Pages/HomePage.g.cpp")
#include "Pages/HomePage.g.cpp"
#endif
#include "../Core/RealmConfig.h"
#include "../Core/RealmStatusChecker.h"
#include "../Core/WowInstall.h"
#include "../Core/CredentialVault.h"
#include "../Core/AppVersion.h"
#include <winrt/Windows.Storage.Pickers.h>
#include <ShObjIdl.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::AzerothCore::Pages::implementation
{
    HomePage::HomePage()
    {
        InitializeComponent();
        CheckRealmStatusAsync();

        auto cfg = Core::RealmConfig::Load();
        WowPathBox().Text(cfg.WowPath);
        RealmAddressBox().Text(cfg.RealmAddress);
        RememberMeCheckBox().IsChecked(cfg.CredentialVaultEnabled);

        // Fill in the account name from a stored credential. Never fill in the
        // password. Do not put a decrypted password into a visible control.
        if (auto cred = Core::CredentialVault::TryGet())
            AccountNameBox().Text(cred->AccountName);

        // Read the version from this executable's own resource. The build
        // writes it there from the release tag.
        VersionTextBlock().Text(L"AzerothCore v" + hstring{ Core::AppVersion::Current() });

        m_loading = false;

        // Storyboards defined in Page.Resources have to be started from here:
        // WinUI3 XAML has no EventTrigger/BeginStoryboard, so there is no
        // declarative way to run them on load.
        Loaded([this](auto&&, auto&&)
            {
                StartAnimation(L"HeroDrift");
                StartAnimation(L"ContentEntrance");
            });
    }

    void HomePage::StartAnimation(std::wstring_view key)
    {
        auto resources = Resources();
        auto boxedKey = box_value(hstring{ key });
        if (!resources.HasKey(boxedKey))
            return;

        if (auto storyboard = resources.Lookup(boxedKey).try_as<Media::Animation::Storyboard>())
            storyboard.Begin();
    }

    void HomePage::ShowError(std::wstring_view message)
    {
        StatusTextBlock().Text(hstring{ message });
        StatusTextBlock().Visibility(Visibility::Visible);
    }

    // WinUI3 panels do not clip their children to their own bounds, and there is
    // no ClipToBounds property to turn that on. The hero image overhangs the left
    // edge by design (see HomePage.xaml) and scales past its box while drifting;
    // without an explicit clip that overhang is painted over the shell around it.
    // Re-cutting the clip on every size change covers the DPI-change case.
    void HomePage::RootGrid_SizeChanged(IInspectable const&, SizeChangedEventArgs const& e)
    {
        Media::RectangleGeometry geometry;
        geometry.Rect({ 0.0f, 0.0f, e.NewSize().Width, e.NewSize().Height });
        RootGrid().Clip(geometry);
    }

    winrt::fire_and_forget HomePage::CheckRealmStatusAsync()
    {
        auto lifetime = get_strong();
        auto cfg = Core::RealmConfig::Load();

        if (cfg.RealmAddress.empty())
        {
            RealmStatusTextBlock().Text(L"No realm configured");
            co_return;
        }

        RealmStatusTextBlock().Text(L"Checking...");

        // CheckAsync returns a raw int32_t rather than RealmReachability directly
        // (see Core/RealmStatusChecker.h: IAsyncOperation<T> requires a
        // winrt::impl::category<T> specialization plain enums don't get).
        auto reachabilityInt = co_await Core::RealmStatusChecker::CheckAsync(cfg.RealmAddress);
        auto reachability = static_cast<Core::RealmReachability>(reachabilityInt);

        DispatcherQueue().TryEnqueue([this, lifetime, reachability]()
            {
                switch (reachability)
                {
                case Core::RealmReachability::Online:
                    RealmStatusTextBlock().Text(L"Online");
                    break;
                case Core::RealmReachability::Unreachable:
                    RealmStatusTextBlock().Text(L"Unreachable");
                    break;
                default:
                    RealmStatusTextBlock().Text(L"No realm configured");
                    break;
                }
            });
    }

    // fire_and_forget rather than void: this has a real co_await (the
    // credential-autofill result) in the Play flow. Everything up to and
    // including LaunchWow() and the RealmConfig::Save() below runs
    // synchronously, before any suspension point, so it's still safe to
    // touch StatusTextBlock (and read AccountNameBox/PasswordBox/
    // RememberMeCheckBox) directly there; `queue` is still captured up
    // front, before the co_await, per the DispatcherQueue()-hoisting pattern
    // in Pages/AddonsPage.cpp's RunSearchAsync -- CredentialVault::
    // AutofillLoginAsync is a Core::Task<T>, which does not preserve the
    // calling thread (see Core/Async.h), and it genuinely does hop off the
    // UI thread today via winrt::resume_after().
    winrt::fire_and_forget HomePage::PlayButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        auto queue = DispatcherQueue();

        auto cfg = Core::RealmConfig::Load();
        if (cfg.WowPath.empty())
        {
            ShowError(L"No WoW install set - use Browse below to point at your Wow.exe");
            co_return;
        }

        // UI control reads -- must happen here, before any co_await below.
        std::wstring accountName = AccountNameBox().Text().c_str();
        std::wstring password = PasswordBox().Password().c_str();
        bool rememberMe = RememberMeCheckBox().IsChecked().GetBoolean();

        bool configSaveFailed = false;
        if (rememberMe && !accountName.empty() && !password.empty())
        {
            Core::CredentialVault::Store(accountName, password);
            cfg.CredentialVaultEnabled = true;
            configSaveFailed = !cfg.Save();
        }

        bool launched = Core::WowInstall::LaunchWow(cfg.WowPath, cfg.RealmAddress);
        if (!launched)
        {
            ShowError(L"Failed to launch WoW - check the install path below");
            co_return;
        }

        if (configSaveFailed)
            ShowError(L"Failed to save settings");
        else
            StatusTextBlock().Visibility(Visibility::Collapsed);

        // Prefer the credentials just typed into the form this session; only
        // fall back to whatever's in the vault if the password field was
        // left empty (e.g. relying on a credential saved in an earlier
        // session).
        bool attemptedAutofill = false;
        bool signedIn = false;
        if (!password.empty())
        {
            attemptedAutofill = true;
            signedIn = co_await Core::CredentialVault::AutofillLoginAsync(accountName, password);
        }
        else if (cfg.CredentialVaultEnabled)
        {
            attemptedAutofill = true;
            signedIn = co_await Core::CredentialVault::AutofillLoginAsync();
        }

        if (attemptedAutofill && !signedIn)
        {
            queue.TryEnqueue([this, lifetime]()
                {
                    ShowError(L"Not signed in - credentials not saved");
                });
        }
    }

    winrt::fire_and_forget HomePage::BrowseButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        // File picker requires the window handle, obtained via WinRT interop.
        auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
        picker.FileTypeFilter().Append(L".exe");
        auto initializeWithWindow = picker.as<::IInitializeWithWindow>();
        HWND hwnd = GetActiveWindow();
        initializeWithWindow->Initialize(hwnd);

        auto file = co_await picker.PickSingleFileAsync();
        if (!file)
            co_return;

        std::wstring path = file.Path().c_str();
        bool valid = Core::WowInstall::IsValidWowExe(path);

        auto cfg = Core::RealmConfig::Load();
        bool saved = false;
        if (valid)
        {
            cfg.WowPath = path;
            saved = cfg.Save();
        }

        DispatcherQueue().TryEnqueue([this, lifetime, path, valid, saved]()
            {
                if (!valid)
                {
                    // Report the rejection. Returning without a message left the
                    // path box unchanged and gave the user no reason.
                    ShowError(L"That isn't a 3.3.5a Wow.exe - pick the Wow.exe in your client folder");
                    return;
                }

                WowPathBox().Text(path);
                if (saved)
                    StatusTextBlock().Visibility(Visibility::Collapsed);
                else
                    ShowError(L"Failed to save settings");
            });
    }

    void HomePage::RealmAddressBox_TextChanged(IInspectable const&, Controls::TextChangedEventArgs const&)
    {
        if (m_loading)
            return;

        auto cfg = Core::RealmConfig::Load();
        cfg.RealmAddress = RealmAddressBox().Text().c_str();
        if (cfg.Save())
            StatusTextBlock().Visibility(Visibility::Collapsed);
        else
            ShowError(L"Failed to save settings");

        // The status line above the fields is now the only place the realm's
        // reachability is reported, so re-check as soon as the address the
        // check depends on changes.
        CheckRealmStatusAsync();
    }

    // This checkbox is the only control for the stored credential. SettingsPage
    // had a separate clear button, and that page is gone. Unchecking therefore
    // deletes the credential. A checkbox marked "Remember me", unchecked, must
    // not leave an encrypted password on disk.
    void HomePage::RememberMeCheckBox_Changed(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loading)
            return;

        bool enabled = RememberMeCheckBox().IsChecked().GetBoolean();

        auto cfg = Core::RealmConfig::Load();
        cfg.CredentialVaultEnabled = enabled;
        if (cfg.Save())
            StatusTextBlock().Visibility(Visibility::Collapsed);
        else
            ShowError(L"Failed to save settings");

        if (!enabled)
            Core::CredentialVault::Clear();
    }
}
