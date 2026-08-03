#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "Pages/HomePage.h"
#include "Pages/AddonsPage.h"
#include "Pages/SettingsPage.h"
#include "Core/RealmConfig.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Media::Animation;

namespace winrt::AzerothCore::implementation
{
    namespace
    {
        // The window is deliberately a single fixed size. The layout is a fixed
        // composition -- hero art, logo and login box arranged against each
        // other -- and it does not degrade gracefully: shrink it and everything
        // gets smushed together. Rather than build responsive breakpoints for a
        // launcher that only ever needs one shape, the window simply refuses to
        // be resized.
        constexpr int32_t kWindowWidth = 1100;
        constexpr int32_t kWindowHeight = 720;
    }

    MainWindow::MainWindow()
    {
        InitializeComponent();
        Title(L"AzerothCore");
        SetupCustomTitleBar();
        SetupFixedWindow();

        // First run / never configured: send the user straight to Settings
        // instead of a Home page that can't do anything useful yet without
        // a realm address and a WoW install path.
        auto cfg = Core::RealmConfig::Load();
        if (cfg.RealmAddress.empty() && cfg.WowPath.empty())
        {
            NavigateTo(xaml_typename<AzerothCore::Pages::SettingsPage>());
            SetActiveNav(NavSettings(), NavSettingsUnderline());
        }
        else
        {
            NavigateTo(xaml_typename<AzerothCore::Pages::HomePage>());
            SetActiveNav(NavHome(), NavHomeUnderline());
        }
    }

    void MainWindow::SetupCustomTitleBar()
    {
        ExtendsContentIntoTitleBar(true);
        // Only the title text is draggable. Handing the whole bar to
        // SetTitleBar would make the nav items un-clickable, since the drag
        // region eats pointer input before it reaches them.
        SetTitleBar(DragRegion());
    }

    void MainWindow::SetupFixedWindow()
    {
        auto appWindow = this->AppWindow();
        appWindow.Resize({ kWindowWidth, kWindowHeight });

        if (auto presenter = appWindow.Presenter().try_as<Microsoft::UI::Windowing::OverlappedPresenter>())
        {
            presenter.IsResizable(false);
            presenter.IsMaximizable(false);
        }

        // Centre on whichever display the window opened on.
        auto area = Microsoft::UI::Windowing::DisplayArea::GetFromWindowId(
            appWindow.Id(), Microsoft::UI::Windowing::DisplayAreaFallback::Nearest);
        if (area)
        {
            auto work = area.WorkArea();
            appWindow.Move({ work.X + (work.Width - kWindowWidth) / 2,
                             work.Y + (work.Height - kWindowHeight) / 2 });
        }
    }

    // Every navigation animates. A launcher that hard-cuts between pages reads
    // as inert; a slide carries the eye across and makes the shell feel like a
    // real application rather than a set of stacked screens.
    void MainWindow::NavigateTo(Windows::UI::Xaml::Interop::TypeName const& page)
    {
        ContentFrame().Navigate(page, nullptr, SlideNavigationTransitionInfo());
    }

    void MainWindow::SetActiveNav(TextBlock const& active, Shapes::Rectangle const& underline)
    {
        auto inactiveBrush = SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xB8, 0xDC, 0xE4, 0xF2));
        auto activeBrush = SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xF0, 0xC8, 0x60));

        for (auto const& nav : { NavHome(), NavAddons(), NavSettings() })
            nav.Foreground(inactiveBrush);
        active.Foreground(activeBrush);

        // Fade the gold underline across rather than snapping it, so the tab
        // change has a visible direction to it.
        for (auto const& u : { NavHomeUnderline(), NavAddonsUnderline(), NavSettingsUnderline() })
        {
            Storyboard sb;
            DoubleAnimation fade;
            fade.To(u == underline ? 1.0 : 0.0);
            fade.Duration(Microsoft::UI::Xaml::Duration{ Windows::Foundation::TimeSpan{ std::chrono::milliseconds(180) } });
            fade.EnableDependentAnimation(true);
            Storyboard::SetTarget(fade, u);
            Storyboard::SetTargetProperty(fade, L"Opacity");
            sb.Children().Append(fade);
            sb.Begin();
        }
    }

    void MainWindow::NavHome_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        NavigateTo(xaml_typename<AzerothCore::Pages::HomePage>());
        SetActiveNav(NavHome(), NavHomeUnderline());
    }

    void MainWindow::NavAddons_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        NavigateTo(xaml_typename<AzerothCore::Pages::AddonsPage>());
        SetActiveNav(NavAddons(), NavAddonsUnderline());
    }

    void MainWindow::NavSettings_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        NavigateTo(xaml_typename<AzerothCore::Pages::SettingsPage>());
        SetActiveNav(NavSettings(), NavSettingsUnderline());
    }
}
