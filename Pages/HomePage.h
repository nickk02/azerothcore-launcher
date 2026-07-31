#pragma once
#include "Pages/HomePage.g.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct HomePage : HomePageT<HomePage>
    {
        HomePage() { InitializeComponent(); }
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct HomePage : HomePageT<HomePage, implementation::HomePage>
    {};
}
