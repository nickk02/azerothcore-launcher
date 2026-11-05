#include "UpdateChecker.h"
#include <cassert>
#include <iostream>

// Only IsNewer is tested here. The network and hashing paths need a real
// GitHub release and a real download, so they are not unit-testable without a
// fixture server; IsNewer is the part that decides whether an update happens at
// all, and it is pure.
int main()
{
    using Core::UpdateChecker;

    // Ordinary release-to-release comparisons.
    {
        assert(UpdateChecker::IsNewer(L"2026.08.07", L"2026.08.06") == true);
        assert(UpdateChecker::IsNewer(L"2026.08.06", L"2026.08.07") == false);
        assert(UpdateChecker::IsNewer(L"2026.08.06", L"2026.08.06") == false);
        assert(UpdateChecker::IsNewer(L"2027.01.01", L"2026.12.31") == true);
    }

    // Numeric, not lexicographic. A string compare gets both of these wrong:
    // "2026.8.9" > "2026.8.10" and "2026.09.01" > "2026.10.01" are both true
    // as text and both false as versions.
    {
        assert(UpdateChecker::IsNewer(L"2026.8.10", L"2026.8.9") == true);
        assert(UpdateChecker::IsNewer(L"2026.8.9", L"2026.8.10") == false);
        assert(UpdateChecker::IsNewer(L"2026.10.01", L"2026.09.01") == true);
    }

    // Leading zeros are just numbers. The release tags carry them.
    {
        assert(UpdateChecker::IsNewer(L"2026.08.06", L"2026.8.6") == false);
        assert(UpdateChecker::IsNewer(L"2026.8.6", L"2026.08.06") == false);
    }

    // A missing trailing component reads as zero, so these are equal.
    {
        assert(UpdateChecker::IsNewer(L"2026.8", L"2026.8.0") == false);
        assert(UpdateChecker::IsNewer(L"2026.8.0", L"2026.8") == false);
        assert(UpdateChecker::IsNewer(L"2026.8.1", L"2026.8") == true);
    }

    // An untagged development build must never be told to update, and must
    // never be treated as newer than a real release. This is the case that
    // matters: 0.0.0-dev is what every local build reports.
    {
        assert(UpdateChecker::IsNewer(L"2026.08.06", L"0.0.0-dev") == false);
        assert(UpdateChecker::IsNewer(L"0.0.0-dev", L"2026.08.06") == false);
        assert(UpdateChecker::IsNewer(L"0.0.0-dev", L"0.0.0-dev") == false);
    }

    // Garbage in either position is never newer. No exceptions, no crashes.
    {
        assert(UpdateChecker::IsNewer(L"", L"2026.08.06") == false);
        assert(UpdateChecker::IsNewer(L"2026.08.06", L"") == false);
        assert(UpdateChecker::IsNewer(L"latest", L"2026.08.06") == false);
        assert(UpdateChecker::IsNewer(L"2026..06", L"2026.08.06") == false);
        assert(UpdateChecker::IsNewer(L"2026.08.06", L"not.a.version") == false);
        assert(UpdateChecker::IsNewer(L"v2026.08.07", L"2026.08.06") == false);
    }

    // A plain 0.0.0, which is what the build stamps without AcVersion, parses
    // fine and is older than any release.
    {
        assert(UpdateChecker::IsNewer(L"2026.08.06", L"0.0.0") == true);
        assert(UpdateChecker::IsNewer(L"0.0.0", L"2026.08.06") == false);
    }

    std::wcout << L"UpdateChecker tests passed\n";
    return 0;
}
