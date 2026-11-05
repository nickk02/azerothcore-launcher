#include "pch.h"
#include "App.xaml.h"
#if __has_include("App.xaml.g.cpp")
#include "App.xaml.g.cpp"
#endif
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::AzerothCore::implementation
{
    App::App()
    {
        InitializeComponent();
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        m_window = make<AzerothCore::implementation::MainWindow>();
        m_window.Activate();
    }
}
