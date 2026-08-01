#pragma once
#include "Pages/HomePage.g.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct HomePage : HomePageT<HomePage>
    {
        HomePage();

        winrt::fire_and_forget CheckRealmStatusAsync();
        void RootGrid_SizeChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const&);
        winrt::fire_and_forget PlayButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct HomePage : HomePageT<HomePage, implementation::HomePage>
    {};
}
