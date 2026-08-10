#include "AddonInstaller.h"

// Windows.Foundation.h carries the operator co_await overloads for WinRT async
// types; Windows.Web.Http.h declares HttpClient but not those. Same trap as
// FelbiteSource.cpp and UpdateChecker.cpp.
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.h>
// DefaultRequestHeaders().UserAgent() lives in the Headers namespace, which is
// a separate header. Without it the accessor is declared but not defined and
// the build fails as C3779, the same shape as the Collections and
// Media.Animation traps recorded elsewhere in this project.
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Storage.Streams.h>

#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <system_error>

using namespace winrt::Windows::Web::Http;
using namespace winrt::Windows::Storage::Streams;

namespace Core
{
    namespace
    {
        // Runs a command and waits. Used for extraction; see ExtractZip.
        bool RunHidden(std::wstring const& commandLine, DWORD timeoutMs)
        {
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi{};

            std::wstring mutableCmd = commandLine;   // CreateProcessW may write to it
            if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
                                CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
            {
                return false;
            }

            DWORD wait = WaitForSingleObject(pi.hProcess, timeoutMs);
            DWORD exitCode = 1;
            if (wait == WAIT_OBJECT_0)
                GetExitCodeProcess(pi.hProcess, &exitCode);
            else
                TerminateProcess(pi.hProcess, 1);

            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return wait == WAIT_OBJECT_0 && exitCode == 0;
        }

        // Extracts with the bsdtar that ships in Windows (10 1803 and later),
        // which reads zip. It is used instead of taking a new zip dependency,
        // and it is pointed at a staging directory rather than at AddOns, so a
        // hostile archive cannot reach the game directory even if bsdtar's own
        // path handling were wrong. The staged result is checked afterwards.
        bool ExtractZip(std::filesystem::path const& archive, std::filesystem::path const& destination)
        {
            wchar_t systemDir[MAX_PATH]{};
            if (GetSystemDirectoryW(systemDir, MAX_PATH) == 0)
                return false;

            std::wstring tar = std::wstring(systemDir) + L"\\tar.exe";
            if (!std::filesystem::exists(tar))
                return false;

            std::wstring cmd = L"\"" + tar + L"\" -xf \"" + archive.wstring()
                             + L"\" -C \"" + destination.wstring() + L"\"";
            return RunHidden(cmd, 120000);
        }

        // A zip starts with "PK\x03\x04". Empty archives use PK\x05\x06.
        bool LooksLikeZip(std::vector<uint8_t> const& bytes)
        {
            return bytes.size() >= 4 && bytes[0] == 'P' && bytes[1] == 'K'
                && (bytes[2] == 0x03 || bytes[2] == 0x05 || bytes[2] == 0x07);
        }

        std::filesystem::path MakeStagingDir()
        {
            wchar_t temp[MAX_PATH]{};
            if (GetTempPathW(MAX_PATH, temp) == 0)
                return {};

            for (int attempt = 0; attempt < 64; ++attempt)
            {
                auto candidate = std::filesystem::path(temp)
                               / (L"ac-addon-" + std::to_wstring(GetTickCount64() + attempt));
                std::error_code ec;
                if (std::filesystem::create_directories(candidate, ec))
                    return candidate;
            }
            return {};
        }
    }

    std::filesystem::path AddonInstaller::ResolveAddOnsDir(std::wstring const& wowExePath)
    {
        if (wowExePath.empty())
            return {};

        return std::filesystem::path(wowExePath).parent_path() / L"Interface" / L"AddOns";
    }

    bool AddonInstaller::IsSafeEntryName(std::wstring const& entryName)
    {
        if (entryName.empty())
            return false;

        // Absolute, rooted, or UNC.
        if (entryName[0] == L'/' || entryName[0] == L'\\')
            return false;
        if (entryName.size() >= 2 && entryName[1] == L':')
            return false;

        // Any ".." component, in either separator style. Checking components
        // rather than searching for ".." as a substring, so a legitimate name
        // like "My..Addon" is not rejected.
        std::wstring component;
        auto check = [&component]() { return component != L".."; };

        for (wchar_t c : entryName)
        {
            if (c == L'/' || c == L'\\')
            {
                if (!check())
                    return false;
                component.clear();
            }
            else
            {
                component += c;
            }
        }
        return check();
    }

    bool AddonInstaller::IsInstalled(std::wstring const& wowExePath, std::wstring const& addonFolderName)
    {
        if (addonFolderName.empty())
            return false;

        auto dir = ResolveAddOnsDir(wowExePath);
        if (dir.empty())
            return false;

        std::error_code ec;
        return std::filesystem::is_directory(dir / addonFolderName, ec);
    }

    Task<InstallResult> AddonInstaller::InstallAsync(RemoteAddon addon, std::wstring wowExePath)
    {
        InstallResult result;
        std::filesystem::path staging;
        std::error_code ec;

        auto cleanup = [&staging, &ec]() {
            if (!staging.empty())
                std::filesystem::remove_all(staging, ec);
        };

        try
        {
            auto addonsDir = ResolveAddOnsDir(wowExePath);
            if (addonsDir.empty())
            {
                result.Status = InstallStatus::NoWowPath;
                result.Message = L"Set your WoW install path first";
                co_return result;
            }

            if (!addon.AddonFolderName.empty() && IsInstalled(wowExePath, addon.AddonFolderName))
            {
                result.Status = InstallStatus::AlreadyInstalled;
                result.Message = L"Already installed";
                co_return result;
            }

            if (addon.DownloadUrl.empty())
            {
                result.Status = InstallStatus::DownloadFailed;
                result.Message = L"This addon has no download link";
                co_return result;
            }

            HttpClient client;
            client.DefaultRequestHeaders().UserAgent().TryParseAdd(L"azerothcore-launcher");
            IBuffer payload = co_await client.GetBufferAsync(
                winrt::Windows::Foundation::Uri(addon.DownloadUrl));

            std::vector<uint8_t> bytes(payload.Length());
            DataReader::FromBuffer(payload).ReadBytes(bytes);

            if (!LooksLikeZip(bytes))
            {
                // Felbite links sometimes land on an HTML interstitial rather
                // than the file. Say so instead of unpacking garbage.
                result.Status = InstallStatus::NotAnArchive;
                result.Message = L"The download was not a zip archive";
                co_return result;
            }

            staging = MakeStagingDir();
            if (staging.empty())
            {
                result.Status = InstallStatus::ExtractFailed;
                result.Message = L"Could not create a temporary directory";
                co_return result;
            }

            auto archivePath = staging / L"addon.zip";
            {
                FILE* file = nullptr;
                if (_wfopen_s(&file, archivePath.c_str(), L"wb") != 0 || !file)
                {
                    result.Status = InstallStatus::ExtractFailed;
                    result.Message = L"Could not write the download";
                    cleanup();
                    co_return result;
                }
                size_t written = fwrite(bytes.data(), 1, bytes.size(), file);
                fclose(file);
                if (written != bytes.size())
                {
                    result.Status = InstallStatus::ExtractFailed;
                    result.Message = L"Could not write the download";
                    cleanup();
                    co_return result;
                }
            }

            auto unpacked = staging / L"unpacked";
            std::filesystem::create_directories(unpacked, ec);

            if (!ExtractZip(archivePath, unpacked))
            {
                result.Status = InstallStatus::ExtractFailed;
                result.Message = L"Could not unpack the archive";
                cleanup();
                co_return result;
            }

            // Only plain top-level directories move across. A zip that unpacked
            // to loose files, or to anything that escaped the staging tree, is
            // refused rather than merged into the game directory.
            std::vector<std::filesystem::path> folders;
            for (auto const& entry : std::filesystem::directory_iterator(unpacked, ec))
            {
                if (!entry.is_directory(ec))
                    continue;

                std::wstring name = entry.path().filename().wstring();
                if (!IsSafeEntryName(name))
                {
                    result.Status = InstallStatus::UnsafeArchive;
                    result.Message = L"The archive contained an unsafe path";
                    cleanup();
                    co_return result;
                }
                folders.push_back(entry.path());
            }

            if (folders.empty())
            {
                result.Status = InstallStatus::NothingToInstall;
                result.Message = L"The archive held no addon folder";
                cleanup();
                co_return result;
            }

            std::filesystem::create_directories(addonsDir, ec);

            for (auto const& folder : folders)
            {
                auto target = addonsDir / folder.filename();
                std::filesystem::remove_all(target, ec);
                std::filesystem::copy(folder, target,
                    std::filesystem::copy_options::recursive, ec);
                if (ec)
                {
                    result.Status = InstallStatus::ExtractFailed;
                    result.Message = L"Could not copy into the AddOns folder";
                    cleanup();
                    co_return result;
                }
                result.InstalledFolders.push_back(folder.filename().wstring());
            }

            result.Status = InstallStatus::Installed;
            result.Message = L"Installed";
            cleanup();
            co_return result;
        }
        catch (...)
        {
            cleanup();
            result.Status = InstallStatus::DownloadFailed;
            result.Message = L"The download failed";
            co_return result;
        }
    }
}
