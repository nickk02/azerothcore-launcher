#pragma once
#include "MainWindow.g.h"

namespace winrt::AzerothCore::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void NavHome_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);
        void NavAddons_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);
        void NavSettings_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);

    private:
        void SetupCustomTitleBar();
        void SetupFixedWindow();
        void SetActiveNav(winrt::Microsoft::UI::Xaml::Controls::TextBlock const& active,
                          winrt::Microsoft::UI::Xaml::Shapes::Rectangle const& underline);
        void NavigateTo(winrt::Windows::UI::Xaml::Interop::TypeName const& page);
    };
}

namespace winrt::AzerothCore::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {};
}
