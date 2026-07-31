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

    Task<AddonSearchResult> AddonCatalog::SearchAsync(std::wstring query)
    {
        AddonSearchResult result;
        for (auto const& source : m_sources)
        {
            try
            {
                auto results = co_await source->SearchAsync(query);
                result.Addons.insert(result.Addons.end(), results.begin(), results.end());
            }
            catch (...)
            {
                // One source failing (e.g. Felbite scraper broke) shouldn't
                // blank out results from any other configured source, but it
                // does need to be recorded: AddonsPage uses this to show a
                // real "addon search unavailable" state instead of an
                // empty-looking "no addons found" list.
                result.AnySourceFailed = true;
            }
        }
        co_return result;
    }
}
