#include "WowInstall.h"
#include <fstream>
#include <windows.h>
#include <shellapi.h>

namespace Core
{
    bool WowInstall::IsValidWowExe(std::filesystem::path const& path)
    {
        return std::filesystem::exists(path)
            && std::filesystem::is_regular_file(path)
            && _wcsicmp(path.filename().c_str(), L"Wow.exe") == 0;
    }

    bool WowInstall::LaunchWow(std::wstring const& exePath, std::wstring const& realmAddress)
    {
        if (!IsValidWowExe(exePath))
            return false;

        std::filesystem::path exe(exePath);
        std::filesystem::path dir = exe.parent_path();

        if (!realmAddress.empty())
        {
            std::filesystem::path realmlist = dir / L"WTF" / L"realmlist.wtf";
            std::filesystem::create_directories(realmlist.parent_path());
            std::wofstream out(realmlist, std::ios::trunc);
            out << L"set realmlist " << realmAddress << L"\n";

            // Also write to locale-specific realmlist files if they exist
            std::filesystem::path dataDir = dir / L"Data";
            if (std::filesystem::exists(dataDir) && std::filesystem::is_directory(dataDir))
            {
                try
                {
                    for (auto const& entry : std::filesystem::directory_iterator(dataDir))
                    {
                        if (entry.is_directory())
                        {
                            std::filesystem::path localeRealmlist = entry.path() / L"realmlist.wtf";
                            std::wofstream localeOut(localeRealmlist, std::ios::trunc);
                            localeOut << L"set realmlist " << realmAddress << L"\n";
                        }
                    }
                }
                catch (...)
                {
                    // Silently ignore errors iterating Data directory
                }
            }
        }

        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.lpFile = exe.c_str();
        sei.lpDirectory = dir.c_str();
        sei.nShow = SW_SHOWNORMAL;
        return ShellExecuteExW(&sei) == TRUE;
    }

    uint64_t WowInstall::GetTotalPlaytimeSeconds(std::wstring const& wowPath)
    {
        // Placeholder data source: reads a small marker file this app maintains
        // itself under the WoW dir's WTF folder, since 3.3.5a doesn't expose
        // playtime any other way without an in-game /played query.
        std::filesystem::path dir = std::filesystem::path(wowPath).parent_path();
        std::filesystem::path marker = dir / L"WTF" / L"azerothcore_playtime.txt";
        std::wifstream in(marker);
        uint64_t seconds = 0;
        if (in.is_open())
            in >> seconds;
        return seconds;
    }

    std::wstring WowInstall::FormatPlaytime(uint64_t seconds)
    {
        uint64_t hours = seconds / 3600;
        uint64_t minutes = (seconds % 3600) / 60;
        return std::to_wstring(hours) + L"h " + std::to_wstring(minutes) + L"m";
    }
}
