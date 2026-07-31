#include "pch.h"
#include "SettingsPage.h"
#if __has_include("Pages/SettingsPage.g.cpp")
#include "Pages/SettingsPage.g.cpp"
#endif
#include "../Core/RealmConfig.h"
#include "../Core/WowInstall.h"
#include "../Core/CredentialVault.h"
#include <winrt/Windows.Storage.Pickers.h>
#include <ShObjIdl.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::AzerothCore::Pages::implementation
{
    SettingsPage::SettingsPage()
    {
        InitializeComponent();
        auto cfg = Core::RealmConfig::Load();
        WowPathBox().Text(cfg.WowPath);
        RealmAddressBox().Text(cfg.RealmAddress);
        CredentialVaultToggle().IsChecked(cfg.CredentialVaultEnabled);
        CredentialFields().Visibility(cfg.CredentialVaultEnabled ? Visibility::Visible : Visibility::Collapsed);

        // Not wired to real build-time codegen yet -- update this string on
        // each dated release. The installer version (installer.iss) already
        // derives its version from the build date automatically; this is a
        // known simplification until the app version is generated the same
        // way at build time.
        VersionTextBlock().Text(L"AzerothCore v2026.07.31");
    }

    winrt::fire_and_forget SettingsPage::BrowseButton_Click(IInspectable const&, RoutedEventArgs const&)
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
        if (!Core::WowInstall::IsValidWowExe(path))
            co_return;

        auto cfg = Core::RealmConfig::Load();
        cfg.WowPath = path;
        bool saved = cfg.Save();

        DispatcherQueue().TryEnqueue([this, lifetime, path, saved]()
            {
                WowPathBox().Text(path);
                if (saved)
                    StatusTextBlock().Visibility(Visibility::Collapsed);
                else
                    ShowSaveError();
            });
    }

    void SettingsPage::RealmAddressBox_TextChanged(IInspectable const&, Controls::TextChangedEventArgs const&)
    {
        auto cfg = Core::RealmConfig::Load();
        cfg.RealmAddress = RealmAddressBox().Text().c_str();
        if (cfg.Save())
            StatusTextBlock().Visibility(Visibility::Collapsed);
        else
            ShowSaveError();
    }

    void SettingsPage::CredentialVaultToggle_Changed(IInspectable const&, RoutedEventArgs const&)
    {
        bool enabled = CredentialVaultToggle().IsChecked().GetBoolean();
        CredentialFields().Visibility(enabled ? Visibility::Visible : Visibility::Collapsed);

        auto cfg = Core::RealmConfig::Load();
        cfg.CredentialVaultEnabled = enabled;
        if (cfg.Save())
            StatusTextBlock().Visibility(Visibility::Collapsed);
        else
            ShowSaveError();

        if (!enabled)
            Core::CredentialVault::Clear();
    }

    void SettingsPage::ShowSaveError()
    {
        StatusTextBlock().Text(L"Failed to save settings");
        StatusTextBlock().Visibility(Visibility::Visible);
    }

    void SettingsPage::SaveCredentialButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        std::wstring account = AccountNameBox().Text().c_str();
        std::wstring password = PasswordBox().Password().c_str();
        if (account.empty() || password.empty())
            return;

        Core::CredentialVault::Store(account, password);
        AccountNameBox().Text(L"");
        PasswordBox().Password(L"");
    }
}
