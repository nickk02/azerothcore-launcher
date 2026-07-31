#pragma once
#include "Pages/SettingsPage.g.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage() { InitializeComponent(); }
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {};
}
