#include "pch.h"
#include "HomePage.h"
#if __has_include("Pages/HomePage.g.cpp")
#include "Pages/HomePage.g.cpp"
#endif
#include "../Core/RealmConfig.h"
#include "../Core/RealmStatusChecker.h"
#include "../Core/WowInstall.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::AzerothCore::Pages::implementation
{
    HomePage::HomePage()
    {
        InitializeComponent();

        auto cfg = Core::RealmConfig::Load();
        if (!cfg.WowPath.empty())
        {
            uint64_t seconds = Core::WowInstall::GetTotalPlaytimeSeconds(cfg.WowPath);
            PlaytimeLabel().Text(Core::WowInstall::FormatPlaytime(seconds));
        }

        CheckRealmStatusAsync();
    }

    winrt::fire_and_forget HomePage::CheckRealmStatusAsync()
    {
        auto lifetime = get_strong();
        auto cfg = Core::RealmConfig::Load();

        if (cfg.RealmAddress.empty())
        {
            RealmStatusTextBlock().Text(L"No realm configured");
            co_return;
        }

        RealmStatusTextBlock().Text(L"Checking...");

        // CheckAsync returns a raw int32_t rather than RealmReachability directly
        // (see Core/RealmStatusChecker.h: IAsyncOperation<T> requires a
        // winrt::impl::category<T> specialization plain enums don't get).
        auto reachabilityInt = co_await Core::RealmStatusChecker::CheckAsync(cfg.RealmAddress);
        auto reachability = static_cast<Core::RealmReachability>(reachabilityInt);

        DispatcherQueue().TryEnqueue([this, lifetime, reachability]()
            {
                switch (reachability)
                {
                case Core::RealmReachability::Online:
                    RealmStatusTextBlock().Text(L"Online");
                    break;
                case Core::RealmReachability::Unreachable:
                    RealmStatusTextBlock().Text(L"Unreachable");
                    break;
                default:
                    RealmStatusTextBlock().Text(L"No realm configured");
                    break;
                }
            });
    }

    void HomePage::PlayButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto cfg = Core::RealmConfig::Load();
        if (cfg.WowPath.empty())
            return; // SettingsPage (Task 9) is where the user sets this; nothing to launch yet.

        Core::WowInstall::LaunchWow(cfg.WowPath, cfg.RealmAddress);
    }
}
