#pragma once
#include "IAddonSource.h"
#include <memory>
#include <vector>

namespace Core
{
    struct AddonCatalog
    {
        AddonCatalog();
        Task<std::vector<RemoteAddon>> SearchAsync(std::wstring query);

    private:
        std::vector<std::unique_ptr<IAddonSource>> m_sources;
    };
}
