#include "AddonCatalog.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

// AddonCatalog fans a search out across every configured source. What matters,
// and what these tests pin down, is that it tells two situations apart:
//
//   - nothing matched          -> AnySourceFailed false, empty results
//   - a source is broken       -> AnySourceFailed true
//
// AddonsPage shows a different message for each ("No addons matched" versus
// "Addon search unavailable"), so getting this wrong means telling the user
// their search found nothing when the scraper is actually down.

namespace
{
    using Core::RemoteAddon;

    // Returns a fixed list.
    struct FakeSource : Core::IAddonSource
    {
        std::wstring name;
        std::vector<RemoteAddon> results;

        FakeSource(std::wstring n, std::vector<RemoteAddon> r)
            : name(std::move(n)), results(std::move(r)) {}

        std::wstring GetName() const override { return name; }

        Core::Task<std::vector<RemoteAddon>> SearchAsync(std::wstring) override
        {
            co_return results;
        }
    };

    // Throws, the way a broken scraper does.
    struct ThrowingSource : Core::IAddonSource
    {
        std::wstring GetName() const override { return L"Broken"; }

        Core::Task<std::vector<RemoteAddon>> SearchAsync(std::wstring) override
        {
            throw std::runtime_error("source is down");
            co_return std::vector<RemoteAddon>{};   // unreachable, makes this a coroutine
        }
    };

    RemoteAddon MakeAddon(std::wstring name, std::wstring source)
    {
        RemoteAddon a;
        a.Name = std::move(name);
        a.SourceName = std::move(source);
        return a;
    }

    // A minimal coroutine to drive a Task<T> from a plain main(). The fakes
    // never hop threads, so everything completes before Run() returns.
    struct Driver
    {
        struct promise_type
        {
            Driver get_return_object() { return {}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() {}
            void unhandled_exception() { std::terminate(); }
        };
    };

    Driver Run(Core::Task<Core::AddonSearchResult> task,
               Core::AddonSearchResult& out, bool& done)
    {
        out = co_await task;
        done = true;
    }

    Core::AddonSearchResult Search(std::vector<std::unique_ptr<Core::IAddonSource>> sources)
    {
        Core::AddonCatalog catalog(std::move(sources));
        Core::AddonSearchResult out;
        bool done = false;
        Run(catalog.SearchAsync(L"query"), out, done);
        assert(done && "the fake sources should complete synchronously");
        return out;
    }
}

int main()
{
    // Two working sources: results concatenate, nothing is marked failed.
    {
        std::vector<std::unique_ptr<Core::IAddonSource>> sources;
        sources.push_back(std::make_unique<FakeSource>(L"A",
            std::vector<RemoteAddon>{ MakeAddon(L"Bartender4", L"A"), MakeAddon(L"DBM", L"A") }));
        sources.push_back(std::make_unique<FakeSource>(L"B",
            std::vector<RemoteAddon>{ MakeAddon(L"Recount", L"B") }));

        auto result = Search(std::move(sources));
        assert(result.Addons.size() == 3);
        assert(result.AnySourceFailed == false);
        assert(result.Addons[0].Name == L"Bartender4");
        assert(result.Addons[2].Name == L"Recount");
    }

    // A working source alongside a broken one. The working source's results
    // must survive, and the failure must still be recorded. Losing either half
    // of this is the bug worth guarding against.
    {
        std::vector<std::unique_ptr<Core::IAddonSource>> sources;
        sources.push_back(std::make_unique<ThrowingSource>());
        sources.push_back(std::make_unique<FakeSource>(L"B",
            std::vector<RemoteAddon>{ MakeAddon(L"Recount", L"B") }));

        auto result = Search(std::move(sources));
        assert(result.Addons.size() == 1);
        assert(result.Addons[0].Name == L"Recount");
        assert(result.AnySourceFailed == true);
    }

    // Order does not matter: a failure after a success is still recorded.
    {
        std::vector<std::unique_ptr<Core::IAddonSource>> sources;
        sources.push_back(std::make_unique<FakeSource>(L"A",
            std::vector<RemoteAddon>{ MakeAddon(L"Bartender4", L"A") }));
        sources.push_back(std::make_unique<ThrowingSource>());

        auto result = Search(std::move(sources));
        assert(result.Addons.size() == 1);
        assert(result.AnySourceFailed == true);
    }

    // Every source broken. This is the "Addon search unavailable" state.
    {
        std::vector<std::unique_ptr<Core::IAddonSource>> sources;
        sources.push_back(std::make_unique<ThrowingSource>());
        sources.push_back(std::make_unique<ThrowingSource>());

        auto result = Search(std::move(sources));
        assert(result.Addons.empty());
        assert(result.AnySourceFailed == true);
    }

    // A working source that matched nothing. Empty results, but NOT a failure.
    // This is the case that must stay distinct from the one above: the UI says
    // "No addons matched" here and "Addon search unavailable" there.
    {
        std::vector<std::unique_ptr<Core::IAddonSource>> sources;
        sources.push_back(std::make_unique<FakeSource>(L"A", std::vector<RemoteAddon>{}));

        auto result = Search(std::move(sources));
        assert(result.Addons.empty());
        assert(result.AnySourceFailed == false);
    }

    // No sources configured at all is also not a failure.
    {
        auto result = Search({});
        assert(result.Addons.empty());
        assert(result.AnySourceFailed == false);
    }

    std::wcout << L"AddonCatalog tests passed\n";
    return 0;
}
