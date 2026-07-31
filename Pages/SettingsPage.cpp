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
    }

    void SettingsPage::BrowseButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        // Deliberately synchronous-looking wrapper kept minimal: file picker
        // requires the window handle, obtained via WinRT interop.
        auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
        picker.FileTypeFilter().Append(L".exe");
        auto initializeWithWindow = picker.as<::IInitializeWithWindow>();
        HWND hwnd = GetActiveWindow();
        initializeWithWindow->Initialize(hwnd);

        auto file = picker.PickSingleFileAsync().get();
        if (!file)
            return;

        std::wstring path = file.Path().c_str();
        if (!Core::WowInstall::IsValidWowExe(path))
            return;

        WowPathBox().Text(path);
        auto cfg = Core::RealmConfig::Load();
        cfg.WowPath = path;
        cfg.Save();
    }

    void SettingsPage::RealmAddressBox_TextChanged(IInspectable const&, Controls::TextChangedEventArgs const&)
    {
        auto cfg = Core::RealmConfig::Load();
        cfg.RealmAddress = RealmAddressBox().Text().c_str();
        cfg.Save();
    }

    void SettingsPage::CredentialVaultToggle_Changed(IInspectable const&, RoutedEventArgs const&)
    {
        bool enabled = CredentialVaultToggle().IsChecked().GetBoolean();
        CredentialFields().Visibility(enabled ? Visibility::Visible : Visibility::Collapsed);

        auto cfg = Core::RealmConfig::Load();
        cfg.CredentialVaultEnabled = enabled;
        cfg.Save();

        if (!enabled)
            Core::CredentialVault::Clear();
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
