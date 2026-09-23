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

## Why this script separates "unreachable" from "unparseable"

An earlier version had one failure path. Any problem raised out of the
fetch and killed the script, so a site outage and a markup change both
surfaced as the same red run. That is the wrong signal: a markup change
needs someone to edit a regex, and an outage needs nobody to do anything.
Six consecutive red days went unread because the red carried no
information.

The exit code now means one thing only:

  exit 1  We fetched a page and could not parse it, or the endpoint
          rejected our request. Someone must change this repository.
  exit 0  Either the parse worked, or the site never answered. Nothing
          here is broken.

A skipped run still prints a warning and writes to the job summary, so it
stays visible without turning the branch red.
"""
import os
import re
import socket
import sys
import time
import urllib.error
import urllib.request

SEARCH_URL = "https://felbite.com/?s=deadly+boss+mods&post_type=addon"
USER_AGENT = "AzerothCoreLauncher-HealthCheck/1.0"

TIMEOUT_SECONDS = 20
ATTEMPTS = 3
BACKOFF_SECONDS = (0, 5, 15)

# A real search page is tens of KB. Cloudflare and origin error pages that
# still answer 200 are far smaller, and treating those as a markup change
# would be a false alarm.
MIN_BODY_BYTES = 2000

CARD_PATTERN = re.compile(
    r'<a class="card card-wide[^"]*" href="([^"]+)">([\s\S]*?)</a>'
)
NAME_PATTERN = re.compile(r'<h5 class="fw-normal mb-0">([^<]+)</h5>')

OK, SKIP, FAILED = "OK", "SKIP", "FAILED"


def annotate(level, message):
    """Emit a GitHub Actions annotation when running in Actions."""
    if os.environ.get("GITHUB_ACTIONS") == "true":
        print(f"::{level}::{message}")


def summarize(message):
    """Append a line to the Actions job summary when running in Actions."""
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if not path:
        return
    try:
        with open(path, "a", encoding="utf-8") as handle:
            handle.write(message + "\n")
    except OSError:
        # The summary is a convenience. Never let it change the verdict.
        pass


def fetch_once(url):
    """Return (body, None) on success, or (None, reason) when unreachable.

    Raises RuntimeError for a response that means this repository is wrong,
    which is not retried because retrying cannot change it.
    """
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
            return response.read().decode("utf-8", errors="replace"), None
    except urllib.error.HTTPError as error:
        # 429 and 5xx are the site's problem and may pass later.
        if error.code == 429 or 500 <= error.code < 600:
            return None, f"HTTP {error.code} from the server"
        # urllib reports a redirect loop as an HTTPError carrying the
        # redirect status. The site is misconfigured, not our code.
        if error.code in (301, 302, 303, 307, 308):
            return None, f"HTTP {error.code} redirect loop"
        raise RuntimeError(
            f"the endpoint answered HTTP {error.code}. "
            f"Check that SEARCH_URL and USER_AGENT in this script are still accepted."
        )
    except urllib.error.URLError as error:
        return None, f"connection failed ({error.reason})"
    except (TimeoutError, socket.timeout):
        return None, f"read timed out after {TIMEOUT_SECONDS}s"
    except OSError as error:
        return None, f"network error ({error})"


def fetch_search_html(url):
    """Retry transient failures, then give up. Returns (body, reason)."""
    reason = "not attempted"
    for attempt in range(ATTEMPTS):
        if BACKOFF_SECONDS[attempt]:
            time.sleep(BACKOFF_SECONDS[attempt])
        body, reason = fetch_once(url)
        if body is not None:
            return body, None
        print(f"  attempt {attempt + 1} of {ATTEMPTS}: {reason}")
    return None, reason


def parse_cards(html):
    cards = CARD_PATTERN.findall(html)
    return [
        (url, name_match.group(1))
        for url, inner in cards
        for name_match in [NAME_PATTERN.search(inner)]
        if name_match
    ]


def check():
    """Return (verdict, message)."""
    try:
        html, reason = fetch_search_html(SEARCH_URL)
    except RuntimeError as error:
        return FAILED, str(error)

    if html is None:
        return SKIP, f"felbite.com did not answer: {reason}. The parser was not checked."

    if len(html) < MIN_BODY_BYTES or "<html" not in html.lower():
        return SKIP, (
            f"felbite.com answered with {len(html)} bytes, which is too small to be "
            f"the search page. Treat this as an outage. The parser was not checked."
        )

    matches = parse_cards(html)
    if not matches:
        return FAILED, (
            "0 results parsed for a known query ('deadly boss mods') from a page that "
            "loaded normally. felbite.com's markup has changed. "
            "Update cardBlockRegex and nameRegex in Core/FelbiteSource.cpp, then update "
            "CARD_PATTERN and NAME_PATTERN here to match."
        )

    return OK, f"parsed {len(matches)} result(s) for the known query."


def main():
    verdict, message = check()
    print(f"[ {verdict} ] Felbite scraper: {message}")

    if verdict == OK:
        return 0
    if verdict == SKIP:
        annotate("warning", f"Felbite health check skipped: {message}")
        summarize(f"### Felbite health check skipped\n\n{message}")
        return 0

    annotate("error", f"Felbite scraper broken: {message}")
    summarize(f"### Felbite scraper broken\n\n{message}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
