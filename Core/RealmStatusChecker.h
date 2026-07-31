#pragma once
#include <string>
#include <utility>
#include <cstdint>
#include <winrt/Windows.Foundation.h>

namespace Core
{
    enum class RealmReachability : int32_t { Unconfigured = 0, Checking = 1, Online = 2, Unreachable = 3 };

    struct RealmStatusChecker
    {
        static std::pair<std::wstring, uint32_t> ParseAddress(std::wstring const& address);
        static winrt::Windows::Foundation::IAsyncOperation<int32_t> CheckAsync(std::wstring address);
    };
}
