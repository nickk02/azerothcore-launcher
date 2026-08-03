#include "pch.h"
#include "HomePage.h"
#if __has_include("Pages/HomePage.g.cpp")
#include "Pages/HomePage.g.cpp"
#endif
#include "../Core/RealmConfig.h"
#include "../Core/RealmStatusChecker.h"
#include "../Core/WowInstall.h"
#include "../Core/CredentialVault.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::AzerothCore::Pages::implementation
{
    HomePage::HomePage()
    {
        InitializeComponent();
        CheckRealmStatusAsync();

        auto cfg = Core::RealmConfig::Load();
        RememberMeCheckBox().IsChecked(cfg.CredentialVaultEnabled);

        // Pre-fill the account name from a previously stored credential, but
        // never the password -- a decrypted password must never be surfaced
        // back into a visible UI control automatically.
        if (auto cred = Core::CredentialVault::TryGet())
            AccountNameBox().Text(cred->AccountName);

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

    // WinUI3 panels do not clip their children to their own bounds, and there is
    // no ClipToBounds property to turn that on. The hero image overhangs the left
    // edge by design (see HomePage.xaml), and without an explicit clip that
    // overhang is painted over MainWindow's nav rail, hiding it entirely --
    // which reads as "the Settings button disappeared" rather than as a drawing
    // bug. Re-cutting the clip on every size change covers window resizes.
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
            StatusTextBlock().Text(L"No WoW install path configured - check Settings");
            StatusTextBlock().Visibility(Visibility::Visible);
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
        else if (!rememberMe)
        {
            // Unchecking Remember me only stops it from being used going
            // forward -- it does not wipe a previously stored credential.
            // Clearing the vault is SettingsPage's explicit affordance.
            cfg.CredentialVaultEnabled = false;
            configSaveFailed = !cfg.Save();
        }

        bool launched = Core::WowInstall::LaunchWow(cfg.WowPath, cfg.RealmAddress);
        if (!launched)
        {
            StatusTextBlock().Text(L"Failed to launch WoW - check your install path in Settings");
            StatusTextBlock().Visibility(Visibility::Visible);
            co_return;
        }

        if (configSaveFailed)
        {
            StatusTextBlock().Text(L"Failed to save settings");
            StatusTextBlock().Visibility(Visibility::Visible);
        }
        else
        {
            StatusTextBlock().Visibility(Visibility::Collapsed);
        }

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
                    StatusTextBlock().Text(L"Not signed in - credentials not saved");
                    StatusTextBlock().Visibility(Visibility::Visible);
                });
        }
    }
}
