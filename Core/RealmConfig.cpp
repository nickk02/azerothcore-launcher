#include "pch.h"
#include "RealmConfig.h"
#include <fstream>
#include <sstream>

namespace Core
{
    std::filesystem::path RealmConfig::ConfigDir()
    {
        wchar_t* appdata = nullptr;
        size_t len = 0;
        _wdupenv_s(&appdata, &len, L"APPDATA");
        std::filesystem::path dir = std::filesystem::path(appdata ? appdata : L".") / L"AzerothCore";
        free(appdata);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }

    RealmConfig RealmConfig::Load()
    {
        RealmConfig cfg;
        std::filesystem::path file = ConfigDir() / L"settings.ini";
        std::wifstream in(file);
        if (!in.is_open())
            return cfg;

        std::wstring line;
        while (std::getline(in, line))
        {
            auto eq = line.find(L'=');
            if (eq == std::wstring::npos)
                continue;
            std::wstring key = line.substr(0, eq);
            std::wstring value = line.substr(eq + 1);
            if (key == L"WowPath")
                cfg.WowPath = value;
            else if (key == L"RealmAddress")
                cfg.RealmAddress = value;
            else if (key == L"CredentialVaultEnabled")
                cfg.CredentialVaultEnabled = (value == L"1");
        }
        return cfg;
    }

    bool RealmConfig::Save() const
    {
        std::filesystem::path file = ConfigDir() / L"settings.ini";
        std::wofstream out(file, std::ios::trunc);
        if (!out.is_open())
            return false;

        out << L"WowPath=" << WowPath << L"\n";
        out << L"RealmAddress=" << RealmAddress << L"\n";
        out << L"CredentialVaultEnabled=" << (CredentialVaultEnabled ? L"1" : L"0") << L"\n";

        if (out.fail())
            return false;

        return true;
    }
}
