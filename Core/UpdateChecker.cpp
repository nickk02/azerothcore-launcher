#include "UpdateChecker.h"
#include "AppVersion.h"

// Windows.Foundation.h carries the operator co_await overloads for WinRT async
// types. Windows.Web.Http.h declares HttpClient but not those overloads, so
// co_awaiting a response fails to compile without it. Same trap as
// FelbiteSource.cpp; the comment there has the full detail.
#include <winrt/Windows.Foundation.h>
// Range-for over a WinRT collection (the assets JsonArray below) needs the
// Collections namespace header. Without it the begin/end for IIterable are
// declared but not defined, which surfaces as C3779 "a function that returns
// 'auto' cannot be used before it is defined" and reads like a language error
// rather than a missing include. Same class of trap as the Media.Animation and
// Shapes headers documented in pch.h.
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Security.Cryptography.Core.h>

#include <windows.h>

// windows.h defines GetObject as a macro for GetObjectA/GetObjectW, so
// IJsonValue::GetObject() below is rewritten to GetObjectW() and fails with
// "'GetObjectW': is not a member of IJsonValue". Same trap as GetCurrentTime,
// which pch.h documents. Nothing here calls the GDI GetObject, so dropping the
// macro costs nothing.
#undef GetObject

#include <shellapi.h>
#include <filesystem>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cwctype>

using namespace winrt::Windows::Web::Http;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Security::Cryptography;
using namespace winrt::Windows::Security::Cryptography::Core;

namespace Core
{
    namespace
    {
        constexpr wchar_t kReleasesApi[] =
            L"https://api.github.com/repos/nickk02/azerothcore-launcher/releases/latest";
        constexpr wchar_t kInstallerAsset[] = L"AzerothCoreSetup.exe";
        constexpr wchar_t kChecksumAsset[]  = L"AzerothCoreSetup.exe.sha256";

        // Splits a dot-separated version into numbers. Returns false if any
        // component is not a plain integer, which is how "0.0.0-dev" and any
        // other non-release string get rejected.
        bool ParseVersion(std::wstring const& text, std::vector<long long>& out)
        {
            out.clear();
            if (text.empty())
                return false;

            std::wistringstream stream(text);
            std::wstring part;
            while (std::getline(stream, part, L'.'))
            {
                if (part.empty())
                    return false;
                for (wchar_t c : part)
                    if (!std::iswdigit(c))
                        return false;
                out.push_back(std::stoll(part));
            }
            return !out.empty();
        }

        // GitHub rejects API requests without a User-Agent.
        HttpClient MakeClient()
        {
            HttpClient client;
            client.DefaultRequestHeaders().UserAgent().TryParseAdd(L"azerothcore-launcher");
            return client;
        }


        // Hashes a file on disk, for deciding whether an already-downloaded
        // installer is the one this release publishes.
        std::wstring HashFileImpl(std::filesystem::path const& path)
        {
            FILE* file = nullptr;
            if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file)
                return {};

            std::vector<uint8_t> bytes;
            uint8_t chunk[64 * 1024];
            size_t read = 0;
            while ((read = fread(chunk, 1, sizeof(chunk), file)) > 0)
                bytes.insert(bytes.end(), chunk, chunk + read);
            fclose(file);

            if (bytes.empty())
                return {};

            auto buffer = CryptographicBuffer::CreateFromByteArray(bytes);
            auto provider = HashAlgorithmProvider::OpenAlgorithm(HashAlgorithmNames::Sha256());
            std::wstring hex = CryptographicBuffer::EncodeToHexString(provider.HashData(buffer)).c_str();
            std::transform(hex.begin(), hex.end(), hex.begin(),
                           [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            return hex;
        }

        // An installer left open by a previous run holds its own file open.
        // That lock is the signal that an update is already in progress.
        bool IsFileLockedImpl(std::filesystem::path const& path)
        {
            HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE)
                return GetLastError() == ERROR_SHARING_VIOLATION;
            CloseHandle(h);
            return false;
        }

        std::wstring ToLowerHex(IBuffer const& buffer)
        {
            std::wstring hex = CryptographicBuffer::EncodeToHexString(buffer).c_str();
            std::transform(hex.begin(), hex.end(), hex.begin(),
                           [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            return hex;
        }
    }

    static std::wstring HashFile(std::filesystem::path const& p) { return HashFileImpl(p); }
    static bool IsFileLocked(std::filesystem::path const& p) { return IsFileLockedImpl(p); }

    bool UpdateChecker::IsNewer(std::wstring const& candidate, std::wstring const& current)
    {
        std::vector<long long> a, b;
        if (!ParseVersion(candidate, a) || !ParseVersion(current, b))
            return false;

        // Compare component by component, treating a missing component as 0 so
        // 2026.8 and 2026.8.0 are equal rather than one being "shorter".
        size_t count = (std::max)(a.size(), b.size());
        for (size_t i = 0; i < count; ++i)
        {
            long long left  = i < a.size() ? a[i] : 0;
            long long right = i < b.size() ? b[i] : 0;
            if (left != right)
                return left > right;
        }
        return false;
    }

    Task<UpdateInfo> UpdateChecker::CheckAsync()
    {
        UpdateInfo info;
        info.Current = AppVersion::Current();

        try
        {
            auto client = MakeClient();
            winrt::hstring body = co_await client.GetStringAsync(
                winrt::Windows::Foundation::Uri(kReleasesApi));

            JsonObject release = JsonObject::Parse(body);

            std::wstring tag = release.GetNamedString(L"tag_name", L"").c_str();
            if (!tag.empty() && (tag[0] == L'v' || tag[0] == L'V'))
                tag.erase(0, 1);
            info.Latest = tag;

            for (auto const& value : release.GetNamedArray(L"assets", JsonArray()))
            {
                JsonObject asset = value.GetObject();
                std::wstring name = asset.GetNamedString(L"name", L"").c_str();
                std::wstring url = asset.GetNamedString(L"browser_download_url", L"").c_str();

                if (name == kInstallerAsset)
                    info.InstallerUrl = url;
                else if (name == kChecksumAsset)
                    info.ChecksumUrl = url;
            }

            // Both assets are required. Offering an update without a checksum
            // would mean running an unverified download, so treat it as no
            // update rather than as a reason to skip verification.
            if (info.InstallerUrl.empty() || info.ChecksumUrl.empty())
            {
                info.Error = L"Release is missing the installer or its checksum";
                co_return info;
            }

            info.Available = IsNewer(info.Latest, info.Current);
        }
        catch (winrt::hresult_error const& e)
        {
            info.Error = e.message().c_str();
        }
        catch (std::exception const&)
        {
            info.Error = L"Update check failed";
        }

        co_return info;
    }

    Task<DownloadResult> UpdateChecker::DownloadVerifiedAsync(UpdateInfo info)
    {
        DownloadResult out;
        std::filesystem::path target;
        std::error_code ec;

        try
        {
            auto client = MakeClient();

            // The checksum file is one line: "<hex>  AzerothCoreSetup.exe".
            winrt::hstring checksumBody = co_await client.GetStringAsync(
                winrt::Windows::Foundation::Uri(info.ChecksumUrl));

            std::wstring expected;
            {
                std::wistringstream stream(std::wstring(checksumBody.c_str()));
                stream >> expected;
            }
            std::transform(expected.begin(), expected.end(), expected.begin(),
                           [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

            if (expected.size() != 64)
            {
                out.Error = L"The release checksum is malformed";
                co_return out;
            }

            wchar_t tempDir[MAX_PATH]{};
            if (GetTempPathW(MAX_PATH, tempDir) == 0)
            {
                out.Error = L"Could not find the temp directory";
                co_return out;
            }
            target = std::filesystem::path(tempDir) / (L"AzerothCoreSetup-" + info.Latest + L".exe");

            // A verified copy may already be here from an earlier run. Reuse it
            // rather than pulling the whole installer down again.
            if (std::filesystem::exists(target, ec))
            {
                if (HashFile(target) == expected)
                {
                    // Locked means a previous run's installer is still open on
                    // it. Starting a second one would stack wizards on the user.
                    if (IsFileLocked(target))
                    {
                        out.Error = L"An update is already running";
                        co_return out;
                    }
                    out.Path = target.wstring();
                    co_return out;
                }
                // Stale or corrupt: drop it and fetch again.
                std::filesystem::remove(target, ec);
            }

            IBuffer payload = co_await client.GetBufferAsync(
                winrt::Windows::Foundation::Uri(info.InstallerUrl));

            auto provider = HashAlgorithmProvider::OpenAlgorithm(HashAlgorithmNames::Sha256());
            std::wstring actual = ToLowerHex(provider.HashData(payload));

            if (actual != expected)
            {
                // Tampered, truncated, or the wrong file. This is the only case
                // that means "do not trust this download".
                out.Error = L"The download failed its checksum";
                co_return out;
            }

            // Write only after the hash matches, so an unverified installer
            // never exists on disk under a name anything might execute.
            auto reader = DataReader::FromBuffer(payload);
            std::vector<uint8_t> bytes(payload.Length());
            reader.ReadBytes(bytes);

            FILE* file = nullptr;
            if (_wfopen_s(&file, target.c_str(), L"wb") != 0 || !file)
            {
                out.Error = L"Could not write the download";
                co_return out;
            }

            size_t written = fwrite(bytes.data(), 1, bytes.size(), file);
            fclose(file);

            if (written != bytes.size())
            {
                std::filesystem::remove(target, ec);
                out.Error = L"Could not write the download";
                co_return out;
            }

            out.Path = target.wstring();
            co_return out;
        }
        catch (...)
        {
            out.Error = L"Could not reach the download";
            co_return out;
        }
    }

    bool UpdateChecker::LaunchInstaller(std::wstring const& installerPath)
    {
        if (installerPath.empty() || !std::filesystem::exists(installerPath))
            return false;

        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOASYNC;   // return only once the launch is committed
        info.lpVerb = L"open";
        info.lpFile = installerPath.c_str();
        info.nShow = SW_SHOWNORMAL;

        if (!ShellExecuteExW(&info))
            return false;

        if (info.hProcess)
            CloseHandle(info.hProcess);

        return true;
    }
}
