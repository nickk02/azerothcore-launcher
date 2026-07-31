#pragma once
#include "Pages/CharactersPage.g.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct CharactersPage : CharactersPageT<CharactersPage>
    {
        CharactersPage() { InitializeComponent(); }
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct CharactersPage : CharactersPageT<CharactersPage, implementation::CharactersPage>
    {};
}
