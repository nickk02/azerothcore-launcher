#pragma once
#include "Pages/AddonsPage.g.h"
#include "../Core/AddonCatalog.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct AddonsPage : AddonsPageT<AddonsPage>
    {
        AddonsPage();

        void SearchButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void SearchBox_KeyDown(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const&);
        void OpenFolderLink_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        winrt::fire_and_forget RunSearchAsync(std::wstring query);

    private:
        Core::AddonCatalog m_catalog;
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct AddonsPage : AddonsPageT<AddonsPage, implementation::AddonsPage>
    {};
}
