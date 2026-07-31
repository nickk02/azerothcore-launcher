#pragma once
#include <string>
#include <optional>
#include <winrt/base.h>

namespace Core
{
    struct StoredCredential { std::wstring AccountName; std::wstring Password; };

    struct CredentialVault
    {
        static void Store(std::wstring const& accountName, std::wstring const& password);
        static std::optional<StoredCredential> TryGet();
        static void Clear();
        static winrt::fire_and_forget AutofillLoginAsync();
    };
}
