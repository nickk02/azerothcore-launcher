#pragma once
#include "MainWindow.g.h"

namespace winrt::AzerothCore::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void NavHome_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);
        void NavAddons_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);
        void NavCharacters_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);
        void NavSettings_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);

    private:
        void SetupCustomTitleBar();
        void SetActiveNav(winrt::Microsoft::UI::Xaml::Controls::TextBlock const& active);
    };
}

namespace winrt::AzerothCore::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {};
}
