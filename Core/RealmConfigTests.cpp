#include "RealmConfig.h"
#include <cassert>
#include <iostream>
#include <filesystem>

int main()
{
    // Isolate from any real user config by pointing HOME/APPDATA-derived path
    // at a throwaway subfolder for this test run.
    auto testDir = std::filesystem::temp_directory_path() / L"AzerothCoreConfigTest";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);
    _wputenv_s(L"APPDATA", testDir.c_str());

    // Load with no file present -> defaults.
    {
        Core::RealmConfig cfg = Core::RealmConfig::Load();
        assert(cfg.WowPath.empty());
        assert(cfg.RealmAddress.empty());
        assert(cfg.CredentialVaultEnabled == false);
    }

    // Save then reload -> round-trips exactly.
    {
        Core::RealmConfig cfg;
        cfg.WowPath = L"C:\\Games\\WoW\\Wow.exe";
        cfg.RealmAddress = L"logon.example.com:3724";
        cfg.CredentialVaultEnabled = true;
        cfg.Save();

        Core::RealmConfig reloaded = Core::RealmConfig::Load();
        assert(reloaded.WowPath == L"C:\\Games\\WoW\\Wow.exe");
        assert(reloaded.RealmAddress == L"logon.example.com:3724");
        assert(reloaded.CredentialVaultEnabled == true);
    }

    std::wcout << L"All RealmConfig tests passed." << std::endl;
    return 0;
}
