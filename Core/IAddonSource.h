#pragma once
#include <string>
#include <vector>
#include "Async.h"

namespace Core
{
    struct RemoteAddon
    {
        std::wstring Id, Name, Description, Author, Version, DownloadUrl, SourceName, Category, ThumbnailUrl, AddonFolderName;
        int DownloadCount = 0;
    };

    struct IAddonSource
    {
        virtual ~IAddonSource() = default;
        virtual std::wstring GetName() const = 0;

        // Returns Core::Task<T> rather than winrt::Windows::Foundation::
        // IAsyncOperation<T> directly: cppwinrt requires T to have a
        // registered ABI category (confirmed via a real build: "TResult must
        // be WinRT type"), and std::vector<T> has no such category for any T
        // -- not just for a plain struct like RemoteAddon. See Async.h for
        // the full explanation and verification notes. Task<T> is still
        // fully co_await-able from WinRT coroutines (AddonCatalog, the
        // health check), so callers use it exactly like an awaitable that
        // returns std::vector<RemoteAddon>.
        virtual Task<std::vector<RemoteAddon>> SearchAsync(std::wstring query) = 0;
    };
}
