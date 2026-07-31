#pragma once
#include <string>
#include <optional>
#include <winrt/base.h>
#include "Async.h"

namespace Core
{
    struct StoredCredential { std::wstring AccountName; std::wstring Password; };

    struct CredentialVault
    {
        static void Store(std::wstring const& accountName, std::wstring const& password);
        static std::optional<StoredCredential> TryGet();
        static void Clear();

        // Returns false (without simulating any keystrokes) when there's no
        // stored credential or the vault access failed -- reuses TryGet()'s
        // existing failure path rather than duplicating it. Callers use this
        // to show an explicit "not signed in" state instead of silently
        // proceeding as if autofill worked (see the design spec's Error
        // handling section). Delegates to the explicit-credential overload
        // below after resolving the stored credential.
        static Task<bool> AutofillLoginAsync();

        // Types the given credentials into the foreground window via
        // SendInput, same mechanism as the no-arg overload above but without
        // reading from the vault first -- used when the caller already has
        // the credentials on hand (e.g. just typed into a login form this
        // session) and doesn't want a Store() round-trip just to autofill.
        static Task<bool> AutofillLoginAsync(std::wstring accountName, std::wstring password);
    };
}
