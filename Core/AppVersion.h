#pragma once
#include <string>

namespace Core
{
    // The running executable's own version, read from its VERSIONINFO resource
    // (see app.rc), which is stamped from the release tag at build time.
    //
    // This exists so the version lives in exactly one place. It used to be a
    // hardcoded wide string in the UI that had to be edited by hand on every
    // release, next to a comment admitting as much; the installer meanwhile
    // derived its own version separately, so the two could and did disagree.
    //
    // An untagged local build reports "0.0.0-dev", which is the honest answer
    // rather than a version number that looks like a release.
    struct AppVersion
    {
        // e.g. "2026.08.06", or "0.0.0-dev" for an untagged build.
        static std::wstring Current();
    };
}
