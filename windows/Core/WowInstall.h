#pragma once
#include <string>
#include <cstdint>
#include <filesystem>

namespace Core
{
    struct WowInstall
    {
        static bool IsValidWowExe(std::filesystem::path const& path);
        static bool LaunchWow(std::wstring const& exePath, std::wstring const& realmAddress);
        static uint64_t GetTotalPlaytimeSeconds(std::wstring const& wowPath);
        static std::wstring FormatPlaytime(uint64_t seconds);
    };
}
