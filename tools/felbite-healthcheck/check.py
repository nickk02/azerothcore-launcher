"""Scheduled health check for the Felbite addon scraper.

Standalone, no dependency on the C++ project: it hits the real felbite.com
search endpoint and checks that the site's markup still matches the shape
FelbiteSource.cpp's ParseSearchResults() expects. This is deliberately
looser than that C++ parser (it only checks that cards exist and have a
name, not every field) since the goal here is "did the site's markup
break", not a full-fidelity reparse.

CARD_PATTERN and NAME_PATTERN mirror the real regexes shipped in
Core/FelbiteSource.cpp:
  - cardBlockRegex: <a class="card card-wide ..." href="...">...</a>
  - nameRegex:      <h5 class="fw-normal mb-0">NAME</h5>

CARD_PATTERN bounds each result card to its own <a>...</a> block before
NAME_PATTERN is checked against it, mirroring FelbiteSource's two-pass
parse. That bound matters: a single unbounded regex scanning the whole
page can, for a malformed/truncated card, span past that card's own
closing tag and pick up a later card's name instead (the cross-card
regex-leak bug found and fixed during FelbiteSource's review). Isolating
each card first makes that leak structurally impossible here too.
"""
import re
import sys
import urllib.request

SEARCH_URL = "https://felbite.com/?s=deadly+boss+mods&post_type=addon"

CARD_PATTERN = re.compile(
    r'<a class="card card-wide[^"]*" href="([^"]+)">([\s\S]*?)</a>'
)
NAME_PATTERN = re.compile(r'<h5 class="fw-normal mb-0">([^<]+)</h5>')


def fetch_search_html(url):
    req = urllib.request.Request(url, headers={"User-Agent": "AzerothCoreLauncher-HealthCheck/1.0"})
    with urllib.request.urlopen(req, timeout=15) as resp:
        return resp.read().decode("utf-8", errors="replace")


def check():
    html = fetch_search_html(SEARCH_URL)
    cards = CARD_PATTERN.findall(html)
    matches = [
        (url, name_match.group(1))
        for url, inner in cards
        for name_match in [NAME_PATTERN.search(inner)]
        if name_match
    ]
    if not matches:
        print("[ FAILED ] Felbite scraper: 0 results parsed for a known query "
              "('deadly boss mods') -- felbite.com's markup has likely changed. "
              "Update the regex in Core/FelbiteSource.cpp to match.")
        return 1
    print(f"[ OK ] Felbite scraper: parsed {len(matches)} result(s) for the known query.")
    return 0


if __name__ == "__main__":
    sys.exit(check())
