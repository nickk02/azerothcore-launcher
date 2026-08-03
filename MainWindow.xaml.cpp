#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "Pages/HomePage.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;

namespace winrt::AzerothCore::implementation
{
    namespace
    {
        // The window is deliberately a single fixed size. The layout is a fixed
        // composition -- hero art with the panel laid over its right third --
        // and it does not degrade gracefully: shrink it and the art is cropped
        // to nothing behind the panel. Rather than build responsive breakpoints
        // for a launcher that only ever needs one shape, the window simply
        // refuses to be resized.
        constexpr int32_t kWindowWidth = 1100;
        constexpr int32_t kWindowHeight = 720;
    }

    MainWindow::MainWindow()
    {
        InitializeComponent();
        Title(L"AzerothCore");
        SetupCustomTitleBar();
        SetupFixedWindow();

        // One destination. Login and setup now live together on HomePage, so
        // there is no first-run branch to take and nothing to navigate between.
        // Pages/AddonsPage.* is still in the project but unreachable on
        // purpose -- addon installation is its own piece of work and was cut
        // from this redesign rather than shipped half-built.
        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::HomePage>());
    }

    void MainWindow::SetupCustomTitleBar()
    {
        ExtendsContentIntoTitleBar(true);
        // The whole bar is the drag region. That only holds while the bar has
        // no interactive children: the drag region eats pointer input before
        // it reaches anything underneath it.
        SetTitleBar(TitleBar());

        // Extending into the title bar does not restyle the system caption
        // buttons, so without this they keep their default light chrome and
        // sit as a bright block on the dark bar.
        auto titleBar = this->AppWindow().TitleBar();
        titleBar.ButtonBackgroundColor(Microsoft::UI::ColorHelper::FromArgb(0x00, 0x00, 0x00, 0x00));
        titleBar.ButtonInactiveBackgroundColor(Microsoft::UI::ColorHelper::FromArgb(0x00, 0x00, 0x00, 0x00));
        titleBar.ButtonForegroundColor(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xDC, 0xE4, 0xF2));
        titleBar.ButtonInactiveForegroundColor(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0x88, 0x8F, 0x99));
        titleBar.ButtonHoverBackgroundColor(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0x2A, 0x2A, 0x2A));
        titleBar.ButtonHoverForegroundColor(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xF0, 0xC8, 0x60));
        titleBar.ButtonPressedBackgroundColor(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0x3A, 0x3A, 0x3A));
        titleBar.ButtonPressedForegroundColor(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xF0, 0xC8, 0x60));
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
}
