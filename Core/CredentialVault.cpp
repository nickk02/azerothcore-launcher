#include "CredentialVault.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Security.Credentials.h>
#include <winrt/Windows.UI.Core.h>
#include <Windows.h>

using namespace winrt::Windows::Security::Credentials;

namespace Core
{
    static constexpr wchar_t kResource[] = L"AzerothCoreLauncher";

    void CredentialVault::Store(std::wstring const& accountName, std::wstring const& password)
    {
        Clear();
        PasswordVault vault;
        vault.Add(PasswordCredential(kResource, accountName, password));
    }

    std::optional<StoredCredential> CredentialVault::TryGet()
    {
        PasswordVault vault;
        try
        {
            auto creds = vault.FindAllByResource(kResource);
            if (creds.Size() == 0)
                return std::nullopt;
            auto first = creds.GetAt(0);
            first.RetrievePassword();
            return StoredCredential{ first.UserName().c_str(), first.Password().c_str() };
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    void CredentialVault::Clear()
    {
        PasswordVault vault;
        try
        {
            auto creds = vault.FindAllByResource(kResource);
            for (auto const& cred : creds)
                vault.Remove(cred);
        }
        catch (...) {} // FindAllByResource throws if nothing is stored yet -- not an error.
    }

    Task<bool> CredentialVault::AutofillLoginAsync()
    {
        auto cred = TryGet();
        if (!cred)
            co_return false;

        co_return co_await AutofillLoginAsync(cred->AccountName, cred->Password);
    }

    Task<bool> CredentialVault::AutofillLoginAsync(std::wstring accountName, std::wstring password)
    {
        // Give the WoW client's login screen time to draw and take focus
        // before simulating keystrokes into whatever window is foreground.
        co_await winrt::resume_after(std::chrono::milliseconds(2500));

        auto sendText = [](std::wstring const& text)
            {
                for (wchar_t ch : text)
                {
                    INPUT input[2] = {};
                    input[0].type = INPUT_KEYBOARD;
                    input[0].ki.wVk = 0;
                    input[0].ki.wScan = ch;
                    input[0].ki.dwFlags = KEYEVENTF_UNICODE;
                    input[1] = input[0];
                    input[1].ki.dwFlags |= KEYEVENTF_KEYUP;
                    SendInput(2, input, sizeof(INPUT));
                }
            };

        sendText(accountName);
        INPUT tab[2] = {};
        tab[0].type = INPUT_KEYBOARD; tab[0].ki.wVk = VK_TAB;
        tab[1] = tab[0]; tab[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, tab, sizeof(INPUT));
        sendText(password);

        co_return true;
    }
}
