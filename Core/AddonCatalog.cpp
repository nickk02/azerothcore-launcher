#include "AddonCatalog.h"
#include "FelbiteSource.h"

namespace Core
{
    AddonCatalog::AddonCatalog()
    {
        m_sources.push_back(std::make_unique<FelbiteSource>());
        // CurseForgeSource() gets added here once Nick has a developer key --
        // deliberately not stubbed out further than this comment (YAGNI: no
        // half-built second source with no key to actually use it).
    }

    Task<std::vector<RemoteAddon>> AddonCatalog::SearchAsync(std::wstring query)
    {
        std::vector<RemoteAddon> combined;
        for (auto const& source : m_sources)
        {
            try
            {
                auto results = co_await source->SearchAsync(query);
                combined.insert(combined.end(), results.begin(), results.end());
            }
            catch (...)
            {
                // One source failing (e.g. Felbite scraper broke) shouldn't
                // blank out results from any other configured source.
            }
        }
        co_return combined;
    }
}
