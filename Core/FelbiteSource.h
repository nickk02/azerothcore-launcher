#pragma once
#include "IAddonSource.h"

namespace Core
{
    // Scrapes felbite.com's search page for 3.3.5a-era addons. There is no
    // official felbite API, so this parses the search-result HTML directly.
    // ParseSearchResults is split out from the network fetch specifically so
    // the parsing logic can be unit-tested against a real HTML fixture
    // without touching the network.
    struct FelbiteSource : IAddonSource
    {
        std::wstring GetName() const override { return L"Felbite"; }
        Task<std::vector<RemoteAddon>> SearchAsync(std::wstring query) override;
        static std::vector<RemoteAddon> ParseSearchResults(std::wstring const& html);
    };
}
