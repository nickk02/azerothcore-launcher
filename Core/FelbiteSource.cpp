#include "FelbiteSource.h"
// <winrt/Windows.Web.Http.h> alone declares HttpClient/GetStringAsync but
// does NOT pull in the operator co_await overloads for WinRT async types --
// those live in <winrt/Windows.Foundation.h> and must be included explicitly
// (confirmed via a real build: co_awaiting GetStringAsync's result failed
// with "'await_resume': is not a member of IAsyncOperationWithProgress<...>"
// without this include). RealmStatusChecker.h gets this for free because it
// already includes Windows.Foundation.h directly for its own return type;
// FelbiteSource.h has no such transitive include since it returns
// Core::Task<T> instead (see Async.h), so it's needed here explicitly.
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.h>
#include <regex>
#include <algorithm>
#include <cwctype>

using namespace winrt::Windows::Web::Http;

namespace Core
{
    namespace
    {
        // felbite.com addon URLs look like ".../addon/4828-deadlybossmods/":
        // a numeric ID, a dash, then the addon's slug. The slug is a
        // reasonable stand-in for the installed addon's folder name until a
        // later task inspects the actual downloaded archive.
        std::wstring SlugFromUrl(std::wstring const& url)
        {
            std::wstring path = url;
            if (!path.empty() && path.back() == L'/')
                path.pop_back();

            auto slashPos = path.find_last_of(L'/');
            std::wstring last = (slashPos == std::wstring::npos) ? path : path.substr(slashPos + 1);

            auto dash = last.find(L'-');
            bool prefixIsNumeric = dash != std::wstring::npos
                && last.find_first_not_of(L"0123456789") == dash;
            if (prefixIsNumeric)
                last = last.substr(dash + 1);

            return last;
        }

        // Parses felbite's "189.6K" / "0.6K" / "1.2M" / "842" download-count
        // text into a plain integer. Never throws: any unparseable input
        // (unexpected formatting, a future markup change, etc.) yields 0
        // rather than propagating an exception out of ParseSearchResults.
        int ParseDownloadCount(std::wstring number, wchar_t suffix)
        {
            // Real numbers on the page never contain thousands separators,
            // but strip commas defensively in case that ever changes.
            number.erase(std::remove(number.begin(), number.end(), L','), number.end());

            try
            {
                double value = std::stod(number);
                switch (towupper(suffix))
                {
                case L'K': value *= 1'000.0; break;
                case L'M': value *= 1'000'000.0; break;
                default: break;
                }
                return static_cast<int>(value);
            }
            catch (...)
            {
                return 0;
            }
        }
    }

    std::vector<RemoteAddon> FelbiteSource::ParseSearchResults(std::wstring const& html)
    {
        std::vector<RemoteAddon> results;

        // Matches felbite.com's real search-result card markup (confirmed
        // against a live fetch of https://felbite.com/?s=...&post_type=addon
        // on 2026-07-31 -- felbite has no official API and no documented
        // markup contract, so this shape is verified ground truth, not a
        // guess):
        //   <a class="card card-wide ..." href="DETAIL_URL">
        //     ...<img ... data-src="THUMBNAIL_URL" ...>...   (src is always
        //                                a throwaway lazyload placeholder)
        //     <h5 class="fw-normal mb-0">NAME</h5>
        //     <p class="text-light fw-normal my-2">DESCRIPTION</p>  (optional)
        //     ...<p class="text-muted mb-0">NUMBER SUFFIX Downloads ...
        static const std::wregex cardRegex(
            LR"RX(<a class="card card-wide[^"]*" href="([^"]+)">[\s\S]*?<img[^>]*data-src="([^"]+)"[^>]*>[\s\S]*?<h5 class="fw-normal mb-0">([^<]+)</h5>(?:[\s\S]*?<p class="text-light fw-normal my-2">([^<]*)</p>)?[\s\S]*?<p class="text-muted mb-0">\s*([\d.,]+)\s*([KMkm]?)\s*Downloads)RX");

        auto begin = std::wsregex_iterator(html.begin(), html.end(), cardRegex);
        auto end = std::wsregex_iterator();
        for (auto it = begin; it != end; ++it)
        {
            auto const& match = *it;

            RemoteAddon addon;
            addon.DownloadUrl = match[1].str();
            addon.ThumbnailUrl = match[2].str();
            addon.Name = match[3].str();
            addon.Description = match[4].matched ? match[4].str() : L"";
            addon.DownloadCount = ParseDownloadCount(match[5].str(), match[6].matched && !match[6].str().empty() ? match[6].str()[0] : L'\0');
            addon.SourceName = L"Felbite";
            addon.AddonFolderName = SlugFromUrl(addon.DownloadUrl);
            addon.Id = L"felbite:" + addon.AddonFolderName;

            results.push_back(std::move(addon));
        }

        return results;
    }

    Task<std::vector<RemoteAddon>> FelbiteSource::SearchAsync(std::wstring query)
    {
        co_await winrt::resume_background();

        std::vector<RemoteAddon> results;
        try
        {
            // Plain, minimal encoding for a search term: felbite's own
            // search form submits spaces as "+", which is what a normal GET
            // form submission of this page produces.
            std::wstring encoded;
            encoded.reserve(query.size());
            for (wchar_t c : query)
                encoded += (c == L' ') ? L'+' : c;

            std::wstring url = L"https://felbite.com/?s=" + encoded + L"&post_type=addon";
            HttpClient client;
            winrt::hstring html = co_await client.GetStringAsync(winrt::Windows::Foundation::Uri(url));
            results = ParseSearchResults(html.c_str());
        }
        catch (...)
        {
            // Network failure, DNS error, HTTP error status, malformed
            // response, etc. -- degrade to an empty result set instead of
            // propagating an exception. AddonCatalog fans this call out
            // across multiple sources; one unreachable source must not take
            // the others down with it.
            results.clear();
        }

        co_return results;
    }
}
