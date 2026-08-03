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

        // Setup fields, folded in from the old SettingsPage.
        winrt::fire_and_forget BrowseButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void RealmAddressBox_TextChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
        void RememberMeCheckBox_Changed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

    private:
        void StartAnimation(std::wstring_view key);
        void ShowError(std::wstring_view message);

        // Seeding the controls from config in the constructor raises the same
        // TextChanged/Checked events a user's own edits do, which would write
        // the config straight back out on every launch -- and, for the
        // Remember me handler, would clear a stored credential on any launch
        // that started out unchecked. Set while the constructor populates the
        // UI; every handler returns early on it.
        bool m_loading{ true };
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct HomePage : HomePageT<HomePage, implementation::HomePage>
    {};
}
