#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "Pages/HomePage.h"
#include "Pages/AddonsPage.h"
#include "Pages/CharactersPage.h"
#include "Pages/SettingsPage.h"
#include "Core/RealmConfig.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;

namespace winrt::AzerothCore::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        Title(L"AzerothCore");
        SetupCustomTitleBar();

        auto appWindow = this->AppWindow();
        appWindow.Resize({ 900, 620 });

        // First run / never configured: send the user straight to Settings
        // instead of a Home page that can't do anything useful yet without
        // a realm address and a WoW install path.
        auto cfg = Core::RealmConfig::Load();
        if (cfg.RealmAddress.empty() && cfg.WowPath.empty())
        {
            ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::SettingsPage>());
            SetActiveNav(NavSettings());
        }
        else
        {
            ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::HomePage>());
        }
    }

    void MainWindow::SetupCustomTitleBar()
    {
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
    }

    void MainWindow::SetActiveNav(TextBlock const& active)
    {
        for (auto const& nav : { NavHome(), NavAddons(), NavCharacters(), NavSettings() })
            nav.Foreground(SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xB8, 0xDC, 0xE4, 0xF2)));
        active.Foreground(SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xF0, 0xC8, 0x60)));
    }

    void MainWindow::NavHome_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::HomePage>());
        SetActiveNav(NavHome());
    }

    void MainWindow::NavAddons_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::AddonsPage>());
        SetActiveNav(NavAddons());
    }

    void MainWindow::NavCharacters_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::CharactersPage>());
        SetActiveNav(NavCharacters());
    }

    void MainWindow::NavSettings_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::SettingsPage>());
        SetActiveNav(NavSettings());
    }
}
