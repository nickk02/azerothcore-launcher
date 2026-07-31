#include "RealmStatusChecker.h"
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <thread>
#include <chrono>

using namespace winrt;
using namespace winrt::Windows::Networking;
using namespace winrt::Windows::Networking::Sockets;

namespace Core
{
    std::pair<std::wstring, uint32_t> RealmStatusChecker::ParseAddress(std::wstring const& address)
    {
        auto colon = address.find(L':');
        if (colon == std::wstring::npos)
            return { address, 3724 };
        std::wstring host = address.substr(0, colon);
        uint32_t port = 3724;
        try { port = std::stoul(address.substr(colon + 1)); } catch (...) {}
        return { host, port };
    }

    winrt::Windows::Foundation::IAsyncOperation<int32_t> RealmStatusChecker::CheckAsync(std::wstring address)
    {
        if (address.empty())
            co_return static_cast<int32_t>(RealmReachability::Unconfigured);

        auto [host, port] = ParseAddress(address);

        co_await winrt::resume_background();

        int32_t result = static_cast<int32_t>(RealmReachability::Unreachable);
        try
        {
            StreamSocket socket;
            HostName hostName{ host };
            auto connectOp = socket.ConnectAsync(hostName, winrt::hstring(std::to_wstring(port)));

            std::thread([connectOp]() mutable
                {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    if (connectOp.Status() == winrt::Windows::Foundation::AsyncStatus::Started)
                        connectOp.Cancel();
                }).detach();

            co_await connectOp;
            result = static_cast<int32_t>(RealmReachability::Online);
            socket.Close();
        }
        catch (...)
        {
            result = static_cast<int32_t>(RealmReachability::Unreachable);
        }

        co_return result;
    }
}
