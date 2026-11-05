#pragma once
#include "Async.h"
#include <string>

namespace Core
{
    // What the newest GitHub release offers, compared against this build.
    struct UpdateInfo
    {
        bool Available = false;          // true only if Latest is newer than Current
        std::wstring Current;            // this executable's version
        std::wstring Latest;             // the newest release tag, without the leading v
        std::wstring InstallerUrl;       // AzerothCoreSetup.exe
        std::wstring ChecksumUrl;        // AzerothCoreSetup.exe.sha256
        std::wstring Error;              // set when the check could not complete
    };

    // Checks GitHub for a newer release, downloads it, and hands off to the
    // installer.
    //
    // The launcher runs an installer it downloaded, so the file is verified
    // against the SHA256 the release publishes before it is executed. A
    // mismatch aborts. Do not remove that check: without it this class
    // downloads an executable over the network and runs it on the strength of
    // a URL alone.
    struct UpdateChecker
    {
        // Reads the newest release from the GitHub API and compares it against
        // AppVersion::Current(). Never throws; failures land in UpdateInfo::Error,
        // because a launcher must still start when GitHub is unreachable.
        static Task<UpdateInfo> CheckAsync();

        // Downloads the installer and checks it against the published SHA256.
        // Returns the path to the verified file, or an empty string. A file
        // that fails the checksum is deleted rather than left on disk.
        static Task<std::wstring> DownloadVerifiedAsync(UpdateInfo info);

        // Starts the installer and returns whether it launched. The caller is
        // expected to close the app straight after: the installer cannot
        // replace files that are still in use.
        static bool LaunchInstaller(std::wstring const& installerPath);

        // True when `candidate` is a strictly newer version than `current`.
        //
        // Versions are dot-separated integers, as produced by the vX.Y.Z release
        // tags. Comparison is numeric per component, so 2026.8.9 beats
        // 2026.08.06 and "10" beats "9", which a string compare would get
        // wrong. A version that does not parse, including the 0.0.0-dev an
        // untagged build reports, is never newer and never older, so a
        // developer build is never told to "update" to a release.
        static bool IsNewer(std::wstring const& candidate, std::wstring const& current);
    };
}
