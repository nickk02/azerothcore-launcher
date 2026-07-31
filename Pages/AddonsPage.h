#pragma once
#include "Pages/AddonsPage.g.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct AddonsPage : AddonsPageT<AddonsPage>
    {
        AddonsPage() { InitializeComponent(); }
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct AddonsPage : AddonsPageT<AddonsPage, implementation::AddonsPage>
    {};
}
