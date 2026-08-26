#include "RealmStatusChecker.h"
#include <cassert>
#include <iostream>

int main()
{
    {
        auto [host, port] = Core::RealmStatusChecker::ParseAddress(L"logon.example.com:3724");
        assert(host == L"logon.example.com");
        assert(port == 3724);
    }
    {
        auto [host, port] = Core::RealmStatusChecker::ParseAddress(L"logon.example.com");
        assert(host == L"logon.example.com");
        assert(port == 3724); // default when no port given
    }
    {
        auto [host, port] = Core::RealmStatusChecker::ParseAddress(L"192.168.1.100:8085");
        assert(host == L"192.168.1.100");
        assert(port == 8085);
    }

    std::wcout << L"All RealmStatusChecker tests passed." << std::endl;
    return 0;
}
