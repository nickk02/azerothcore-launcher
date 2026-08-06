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
        // One fixed size. The layout is a fixed composition: hero art with the
        // panel over its right third. Shrink it and the panel crops the art to
        // nothing. The launcher needs one shape, so the window does not resize.
        constexpr int32_t kWindowWidth = 1100;
        constexpr int32_t kWindowHeight = 720;
    }

    MainWindow::MainWindow()
    {
        InitializeComponent();
        Title(L"AzerothCore");
        SetupCustomTitleBar();
        SetupFixedWindow();

        // One destination. Login and setup share HomePage, so there is no
        // first-run branch and nothing to navigate between. Pages/AddonsPage.*
        // still compiles, but nothing opens it. Addon installation is a
        // separate job.
        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::HomePage>());
    }

    void MainWindow::SetupCustomTitleBar()
    {
        ExtendsContentIntoTitleBar(true);
        // The whole bar is the drag region. This works only while the bar has
        // no interactive children. The drag region takes pointer input before
        // any child sees it.
        SetTitleBar(TitleBar());

        // Extending into the title bar does not restyle the caption buttons.
        // Without this they stay light and sit as a bright block on a dark bar.
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

        // AppWindow sizes are physical pixels. kWindowWidth and kWindowHeight
        // are logical units, so multiply them by the display scale.
        //
        // This did not matter while the process was DPI-unaware. The operating
        // system enlarged 1100 physical to 2200 on a 200% display, so the window
        // landed at the correct size by accident. app.manifest now declares
        // PerMonitorV2, so no enlargement happens and an unscaled Resize gives a
        // half-size window.
        //
        // The DPI is read once, for the display the window opens on. Moving the
        // window to a monitor with a different scale would invalidate it. That
        // case is not handled: the window cannot be resized, and nothing else in
        // the app responds to a DPI change.
        HWND hwnd{};
        double scale = 1.0;
        if (auto native = this->try_as<::IWindowNative>();
            native && SUCCEEDED(native->get_WindowHandle(&hwnd)) && hwnd)
        {
            if (UINT dpi = GetDpiForWindow(hwnd); dpi != 0)
                scale = dpi / 96.0;
        }

        const int32_t width = static_cast<int32_t>(kWindowWidth * scale);
        const int32_t height = static_cast<int32_t>(kWindowHeight * scale);
        appWindow.Resize({ width, height });

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
            appWindow.Move({ work.X + (work.Width - width) / 2,
                             work.Y + (work.Height - height) / 2 });
        }
    }
}
