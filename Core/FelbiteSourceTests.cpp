#include "FelbiteSource.h"
#include <cassert>
#include <iostream>

int main()
{
    // Fixture: two real result cards, verbatim (trimmed of surrounding page
    // chrome) from a live fetch of https://felbite.com/?s=deadly+boss+mods&post_type=addon
    // on 2026-07-31. felbite.com has no official API and no documented HTML
    // contract, so this fixture is the ground truth for what ParseSearchResults
    // must handle -- NOT a guess at the markup shape.
    std::wstring html = LR"RX(
        <a class="card card-wide bg-dark rounded-6 py-2" href="https://felbite.com/addon/4828-deadlybossmods/">
            <div class="wrapper">
                <div class="image">
                    <img class="img-fluid rounded-circle lazyload" data-src="https://felbite.com/wp-content/uploads/2022/03/felbite.com-deadlybossmods-logo-deadlybossmods.webp" alt="DeadlyBossMods Download" src="data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMSIgaGVpZ2h0PSIxIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciPjwvc3ZnPg==" style="--smush-placeholder-width: 280px; --smush-placeholder-aspect-ratio: 280/280;">
                </div>
                <div class="details">
                    <div class="left-side align-self-center">
                        <h5 class="fw-normal mb-0">DeadlyBossMods</h5>
                        <p class="text-light fw-normal my-2">Alerts you when a Boss begins to cast certain spells or use certain skills.</p>
                        <div>
                            <p class="text-muted mb-0">189.6K                        Downloads &bull; 278.3K Views
                            </p>
                        </div>
                    </div>
                    <div class="right-side align-self-center">
                        <div class="stats d-flex flex-wrap text-center">
                            <li class="expansion rounded d-inline-block text-uppercase py-1 px-2" data-bs-toggle="tooltip" data-bs-placement="bottom"
                                title="Wrath of the Lich King">wotlk                    </li>
                        </div>
                    </div>
                </div>
            </div>
        </a>
        <a class="card card-wide bg-dark rounded-6 py-2" href="https://felbite.com/addon/3583-ouroloot/">
            <div class="wrapper">
                <div class="image">
                    <img class="img-fluid rounded-circle lazyload" data-src="https://felbite.com/wp-content/uploads/2022/03/felbite.com-ouroloot-logo-ouroloot.jpg" alt="OuroLoot Download" src="data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMSIgaGVpZ2h0PSIxIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciPjwvc3ZnPg==" style="--smush-placeholder-width: 855px; --smush-placeholder-aspect-ratio: 855/542;">
                </div>
                <div class="details">
                    <div class="left-side align-self-center">
                        <h5 class="fw-normal mb-0">OuroLoot</h5>
                        <p class="text-light fw-normal my-2">Ouro Loot helps you easily tracks loot during a raid.</p>
                        <div>
                            <p class="text-muted mb-0">0.6K                        Downloads &bull; 9.8K Views
                            </p>
                        </div>
                    </div>
                    <div class="right-side align-self-center">
                        <div class="stats d-flex flex-wrap text-center">
                            <li class="expansion rounded d-inline-block text-uppercase py-1 px-2" data-bs-toggle="tooltip" data-bs-placement="bottom"
                                title="Wrath of the Lich King">wotlk                    </li>
                        </div>
                    </div>
                </div>
            </div>
        </a>
    )RX";

    auto results = Core::FelbiteSource::ParseSearchResults(html);
    assert(results.size() == 2);

    assert(results[0].Name == L"DeadlyBossMods");
    assert(results[0].SourceName == L"Felbite");
    assert(results[0].Description == L"Alerts you when a Boss begins to cast certain spells or use certain skills.");
    assert(results[0].DownloadUrl == L"https://felbite.com/addon/4828-deadlybossmods/");
    // Real thumbnail lives in data-src (lazy-loaded); the img's src attribute
    // is always a throwaway base64 SVG placeholder on the live site and must
    // never be picked up here.
    assert(results[0].ThumbnailUrl == L"https://felbite.com/wp-content/uploads/2022/03/felbite.com-deadlybossmods-logo-deadlybossmods.webp");
    assert(results[0].AddonFolderName == L"deadlybossmods");
    assert(results[0].Id == L"felbite:deadlybossmods");
    assert(results[0].DownloadCount == 189600); // "189.6K"

    assert(results[1].Name == L"OuroLoot");
    assert(results[1].AddonFolderName == L"ouroloot");
    assert(results[1].DownloadCount == 600); // "0.6K"

    // Empty/unrecognized HTML -> empty result, not a crash.
    auto empty = Core::FelbiteSource::ParseSearchResults(L"<html><body>no results</body></html>");
    assert(empty.empty());

    // A malformed/truncated card (no <h5> name, e.g. a partially-rendered
    // page or a markup change upstream) must be skipped, not crash or
    // produce a garbage entry.
    std::wstring malformed = LR"RX(
        <a class="card card-wide bg-dark rounded-6 py-2" href="https://felbite.com/addon/9999-broken/">
            <div class="wrapper"><div class="image"><img class="img-fluid" data-src="https://felbite.com/thumb.png"></div></div>
        </a>
    )RX";
    auto malformedResults = Core::FelbiteSource::ParseSearchResults(malformed);
    assert(malformedResults.empty());

    // Regression test for the cross-card leak: a malformed card (no <h5>,
    // same as above) immediately followed by a well-formed one. Before
    // ParseSearchResults bounded each card to its own <a ...>...</a> block,
    // a single unbounded lazy [\s\S]*? scan starting at the malformed card's
    // opening tag could skip straight past its "</a>" and pick up the NEXT
    // card's <h5>/downloads paragraph, producing a corrupted entry that
    // pairs the malformed card's own href/thumbnail with the good card's
    // name and download count. With the fix, the malformed card contributes
    // no entry at all, and the good card is parsed as its own isolated
    // block -- its fields must come entirely from ITS OWN markup, not
    // leaked from the broken card in front of it.
    std::wstring adjacent = LR"RX(
        <a class="card card-wide bg-dark rounded-6 py-2" href="https://felbite.com/addon/9999-broken/">
            <div class="wrapper"><div class="image"><img class="img-fluid" data-src="https://felbite.com/thumb-broken.png"></div></div>
        </a>
        <a class="card card-wide bg-dark rounded-6 py-2" href="https://felbite.com/addon/3583-ouroloot/">
            <div class="wrapper">
                <div class="image">
                    <img class="img-fluid rounded-circle lazyload" data-src="https://felbite.com/wp-content/uploads/2022/03/felbite.com-ouroloot-logo-ouroloot.jpg" alt="OuroLoot Download" src="data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMSIgaGVpZ2h0PSIxIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciPjwvc3ZnPg==">
                </div>
                <div class="details">
                    <div class="left-side align-self-center">
                        <h5 class="fw-normal mb-0">OuroLoot</h5>
                        <p class="text-light fw-normal my-2">Ouro Loot helps you easily tracks loot during a raid.</p>
                        <div>
                            <p class="text-muted mb-0">0.6K                        Downloads &bull; 9.8K Views
                            </p>
                        </div>
                    </div>
                </div>
            </div>
        </a>
    )RX";
    auto adjacentResults = Core::FelbiteSource::ParseSearchResults(adjacent);
    assert(adjacentResults.size() == 1);
    assert(adjacentResults[0].Name == L"OuroLoot");
    assert(adjacentResults[0].DownloadUrl == L"https://felbite.com/addon/3583-ouroloot/");
    assert(adjacentResults[0].ThumbnailUrl == L"https://felbite.com/wp-content/uploads/2022/03/felbite.com-ouroloot-logo-ouroloot.jpg");
    assert(adjacentResults[0].DownloadCount == 600);

    std::wcout << L"All FelbiteSource tests passed." << std::endl;
    return 0;
}
