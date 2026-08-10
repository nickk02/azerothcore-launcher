#include "AddonInstaller.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>

// The network and extraction paths need a real download and the system tar, so
// they are not unit-tested here. The two pure functions are, and one of them is
// the security control for the whole feature.
int main()
{
    using Core::AddonInstaller;

    // ResolveAddOnsDir: the AddOns folder sits beside the executable.
    {
        auto dir = AddonInstaller::ResolveAddOnsDir(L"C:\\Games\\WoW\\Wow.exe");
        assert(dir == std::filesystem::path(L"C:\\Games\\WoW\\Interface\\AddOns"));

        assert(AddonInstaller::ResolveAddOnsDir(L"").empty());
    }

    // IsSafeEntryName: the zip-slip control. Everything here is an archive
    // entry that a hostile or broken addon zip could contain.
    {
        // Ordinary names pass.
        assert(AddonInstaller::IsSafeEntryName(L"Bartender4") == true);
        assert(AddonInstaller::IsSafeEntryName(L"Deadly Boss Mods") == true);
        assert(AddonInstaller::IsSafeEntryName(L"DBM-Core/Locales") == true);
        assert(AddonInstaller::IsSafeEntryName(L"DBM-Core\\Locales") == true);

        // A dot in a name is fine. Only a whole ".." component is not, which is
        // why this checks components rather than searching for the substring.
        assert(AddonInstaller::IsSafeEntryName(L"My..Addon") == true);
        assert(AddonInstaller::IsSafeEntryName(L"Addon.v2") == true);
        assert(AddonInstaller::IsSafeEntryName(L"..Addon") == true);

        // Escapes upward, in both separator styles.
        assert(AddonInstaller::IsSafeEntryName(L"..") == false);
        assert(AddonInstaller::IsSafeEntryName(L"../evil") == false);
        assert(AddonInstaller::IsSafeEntryName(L"..\\evil") == false);
        assert(AddonInstaller::IsSafeEntryName(L"a/../../evil") == false);
        assert(AddonInstaller::IsSafeEntryName(L"a\\..\\..\\Windows\\System32\\evil.dll") == false);
        assert(AddonInstaller::IsSafeEntryName(L"good/../..") == false);

        // Absolute and rooted paths.
        assert(AddonInstaller::IsSafeEntryName(L"/etc/passwd") == false);
        assert(AddonInstaller::IsSafeEntryName(L"\\Windows\\System32") == false);
        assert(AddonInstaller::IsSafeEntryName(L"C:\\Windows\\System32\\evil.dll") == false);
        assert(AddonInstaller::IsSafeEntryName(L"D:relative") == false);

        // UNC.
        assert(AddonInstaller::IsSafeEntryName(L"\\\\server\\share\\evil") == false);

        // Empty.
        assert(AddonInstaller::IsSafeEntryName(L"") == false);
    }

    // IsInstalled: reports on a real directory.
    {
        auto root = std::filesystem::temp_directory_path() / L"AddonInstallerTest";
        std::filesystem::remove_all(root);

        auto exe = root / L"Wow.exe";
        std::filesystem::create_directories(root);
        std::ofstream(exe).put('\0');

        assert(AddonInstaller::IsInstalled(exe.wstring(), L"Bartender4") == false);

        std::filesystem::create_directories(root / L"Interface" / L"AddOns" / L"Bartender4");
        assert(AddonInstaller::IsInstalled(exe.wstring(), L"Bartender4") == true);

        // A file of the same name is not an installed addon.
        std::ofstream(root / L"Interface" / L"AddOns" / L"NotAFolder").put('\0');
        assert(AddonInstaller::IsInstalled(exe.wstring(), L"NotAFolder") == false);

        // An empty folder name is never installed.
        assert(AddonInstaller::IsInstalled(exe.wstring(), L"") == false);

        std::filesystem::remove_all(root);
    }

    std::wcout << L"AddonInstaller tests passed\n";
    return 0;
}
