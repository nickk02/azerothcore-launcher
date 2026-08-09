#pragma once
#include "Async.h"
#include "IAddonSource.h"
#include <filesystem>
#include <string>
#include <vector>

namespace Core
{
    enum class InstallStatus
    {
        Installed,
        AlreadyInstalled,
        NoWowPath,        // the client path is not configured
        DownloadFailed,
        NotAnArchive,     // the download was not a zip
        UnsafeArchive,    // the archive tried to write outside AddOns
        ExtractFailed,
        NothingToInstall  // the archive held no addon folder
    };

    struct InstallResult
    {
        InstallStatus Status = InstallStatus::ExtractFailed;
        std::vector<std::wstring> InstalledFolders;   // what landed in AddOns
        std::wstring Message;                         // for the UI
    };

    // Installs an addon into <client>\Interface\AddOns.
    //
    // This downloads a zip from a third-party site and unpacks it into the
    // user's game directory, so the extraction is treated as hostile input.
    // Nothing is written into AddOns directly. The archive is unpacked into a
    // temporary staging directory first, the result is checked, and only plain
    // top-level folders are moved across. See IsSafeEntryName for what that
    // rejects and why.
    struct AddonInstaller
    {
        // <client dir>\Interface\AddOns for a given Wow.exe path. Empty if the
        // path is empty. Does not create anything.
        static std::filesystem::path ResolveAddOnsDir(std::wstring const& wowExePath);

        // Rejects any archive entry that could escape the directory it is
        // unpacked into: absolute paths, drive letters, UNC paths, and any
        // component that is "..".
        //
        // This is the zip-slip check. An archive entry named
        // "..\..\Windows\System32\evil.dll" extracts outside the target
        // directory in a naive implementation, and this addon content is
        // downloaded from a scraped third-party site. Do not remove it.
        static bool IsSafeEntryName(std::wstring const& entryName);

        // True if a folder of that name already exists under AddOns.
        static bool IsInstalled(std::wstring const& wowExePath, std::wstring const& addonFolderName);

        // Downloads, verifies and installs. Never throws.
        static Task<InstallResult> InstallAsync(RemoteAddon addon, std::wstring wowExePath);
    };
}
