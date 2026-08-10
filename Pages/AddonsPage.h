#pragma once
#include "Pages/AddonsPage.g.h"
#include "../Core/AddonCatalog.h"
#include "../Core/IAddonSource.h"

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
        // Builds one result row: name and source, a status line, and an Install
        // button bound to this addon.
        winrt::Microsoft::UI::Xaml::UIElement BuildResultRow(Core::RemoteAddon const& addon);

        // Runs the install and reports into that row's own status text.
        winrt::fire_and_forget InstallAddonAsync(
            Core::RemoteAddon addon,
            winrt::Microsoft::UI::Xaml::Controls::Button button,
            winrt::Microsoft::UI::Xaml::Controls::TextBlock status);

        Core::AddonCatalog m_catalog;
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct AddonsPage : AddonsPageT<AddonsPage, implementation::AddonsPage>
    {};
}
