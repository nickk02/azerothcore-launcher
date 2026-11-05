#pragma once
#include <string>

namespace Core
{
    // Returns this executable's version, read from its VERSIONINFO resource.
    // See app.rc. The build writes that resource from the release tag.
    //
    // The version therefore has one source. It used to be a hardcoded string in
    // the user interface, and the installer took its version from the build date
    // separately. The two could disagree, and they did.
    //
    // An untagged build reports "0.0.0-dev". It must not report a version that
    // looks like a release.
    struct AppVersion
    {
        // e.g. "2026.08.06", or "0.0.0-dev" for an untagged build.
        static std::wstring Current();
    };
}
