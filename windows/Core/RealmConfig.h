#pragma once
#include <string>
#include <filesystem>

namespace Core
{
    struct RealmConfig
    {
        std::wstring WowPath;
        std::wstring RealmAddress;
        bool CredentialVaultEnabled = false;

        static RealmConfig Load();
        bool Save() const;
        static std::filesystem::path ConfigDir();
    };
}
