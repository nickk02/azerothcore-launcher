#pragma once
#include "IAddonSource.h"
#include <memory>
#include <vector>

namespace Core
{
    // Result of a fanned-out search across every configured IAddonSource.
    // AnySourceFailed lets AddonsPage tell "zero results" (nothing matched)
    // apart from "a source is broken" (e.g. the Felbite scraper is down) --
    // see the design spec's Error handling section.
    struct AddonSearchResult
    {
        std::vector<RemoteAddon> Addons;
        bool AnySourceFailed = false;
    };

    struct AddonCatalog
    {
        AddonCatalog();
        Task<AddonSearchResult> SearchAsync(std::wstring query);

    private:
        std::vector<std::unique_ptr<IAddonSource>> m_sources;
    };
}
