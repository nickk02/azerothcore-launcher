#include "CredentialVault.h"
#include <cassert>
#include <iostream>

int main()
{
    // Clean slate.
    Core::CredentialVault::Clear();
    assert(Core::CredentialVault::TryGet().has_value() == false);

    // Store then retrieve.
    Core::CredentialVault::Store(L"testaccount", L"correcthorsebatterystaple");
    auto cred = Core::CredentialVault::TryGet();
    assert(cred.has_value());
    assert(cred->AccountName == L"testaccount");
    assert(cred->Password == L"correcthorsebatterystaple");

    // Overwrite.
    Core::CredentialVault::Store(L"testaccount2", L"differentpassword");
    auto cred2 = Core::CredentialVault::TryGet();
    assert(cred2->AccountName == L"testaccount2");
    assert(cred2->Password == L"differentpassword");

    // Clear.
    Core::CredentialVault::Clear();
    assert(Core::CredentialVault::TryGet().has_value() == false);

    std::wcout << L"All CredentialVault tests passed." << std::endl;
    return 0;
}
