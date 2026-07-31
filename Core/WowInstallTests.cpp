#include "WowInstall.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>

int main()
{
    // IsValidWowExe: rejects missing files, non-exe files, accepts a real
    // (fake, for the test) Wow.exe.
    {
        auto testDir = std::filesystem::temp_directory_path() / L"AzerothCoreWowInstallTest";
        std::filesystem::remove_all(testDir);
        std::filesystem::create_directories(testDir);

        assert(Core::WowInstall::IsValidWowExe(testDir / L"Wow.exe") == false); // doesn't exist yet

        std::ofstream(testDir / L"Wow.exe").put('\0');
        assert(Core::WowInstall::IsValidWowExe(testDir / L"Wow.exe") == true);

        std::ofstream(testDir / L"NotWow.exe").put('\0');
        assert(Core::WowInstall::IsValidWowExe(testDir / L"NotWow.exe") == false); // wrong filename
    }

    // FormatPlaytime: pure formatting, no filesystem involved.
    {
        assert(Core::WowInstall::FormatPlaytime(0) == L"0h 0m");
        assert(Core::WowInstall::FormatPlaytime(59) == L"0h 0m");
        assert(Core::WowInstall::FormatPlaytime(3600) == L"1h 0m");
        assert(Core::WowInstall::FormatPlaytime(3661) == L"1h 1m");
        assert(Core::WowInstall::FormatPlaytime(90000) == L"25h 0m");
    }

    std::wcout << L"All WowInstall tests passed." << std::endl;
    return 0;
}
