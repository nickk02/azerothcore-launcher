#include "AppVersion.h"
#include <windows.h>
#include <vector>

#pragma comment(lib, "version.lib")

namespace Core
{
    namespace
    {
        constexpr wchar_t kUnknown[] = L"0.0.0-dev";

        std::wstring ReadFileVersionString()
        {
            // Ask for the module path rather than assuming a fixed length:
            // GetModuleFileNameW truncates silently and still reports success
            // on older behaviour, so grow until it fits.
            std::vector<wchar_t> path(MAX_PATH);
            for (;;)
            {
                DWORD copied = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
                if (copied == 0)
                    return kUnknown;
                if (copied < path.size() - 1)
                    break;
                path.resize(path.size() * 2);
            }

            DWORD handle = 0;
            DWORD size = GetFileVersionInfoSizeW(path.data(), &handle);
            if (size == 0)
                return kUnknown;   // no version resource, e.g. an art-free CI build predating this

            std::vector<std::byte> buffer(size);
            if (!GetFileVersionInfoW(path.data(), 0, size, buffer.data()))
                return kUnknown;

            // Read the translation table rather than hardcoding 040904b0. The
            // block name has to match the language/codepage actually present or
            // VerQueryValue returns nothing, and that is an easy way to end up
            // with a version that works on one machine's build and not another.
            struct LangCodepage { WORD language; WORD codePage; };
            LangCodepage* translations = nullptr;
            UINT translationBytes = 0;
            if (!VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation",
                                reinterpret_cast<void**>(&translations), &translationBytes)
                || translationBytes < sizeof(LangCodepage))
            {
                return kUnknown;
            }

            wchar_t subBlock[64];
            swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\FileVersion",
                       translations[0].language, translations[0].codePage);

            wchar_t* value = nullptr;
            UINT valueChars = 0;
            if (!VerQueryValueW(buffer.data(), subBlock, reinterpret_cast<void**>(&value), &valueChars)
                || valueChars == 0)
            {
                return kUnknown;
            }

            // valueChars counts the terminator; trimming it avoids an embedded
            // NUL riding along into the UI string.
            return std::wstring(value, valueChars - 1);
        }
    }

    std::wstring AppVersion::Current()
    {
        // Read once. The value cannot change while the process is alive, and
        // this is called from UI construction.
        static const std::wstring version = ReadFileVersionString();
        return version;
    }
}
