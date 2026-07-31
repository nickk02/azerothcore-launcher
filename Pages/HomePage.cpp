#include "pch.h"
#include "HomePage.h"
#if __has_include("Pages/HomePage.g.cpp")
#include "Pages/HomePage.g.cpp"
#endif
#include "../Core/RealmConfig.h"
#include "../Core/RealmStatusChecker.h"
#include "../Core/WowInstall.h"
#include "../Core/CredentialVault.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::AzerothCore::Pages::implementation
{
    HomePage::HomePage()
    {
        InitializeComponent();
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

    // fire_and_forget rather than void: this now has a real co_await (the
    // credential-autofill result) in the Play flow, the first one in this
    // file. Everything up to and including LaunchWow() runs synchronously,
    // before any suspension point, so it's still safe to touch StatusTextBlock
    // directly there; `queue` is still captured up front, before the
    // co_await, per the DispatcherQueue()-hoisting pattern in
    // Pages/AddonsPage.cpp's RunSearchAsync -- CredentialVault::AutofillLoginAsync
    // is a Core::Task<T>, which does not preserve the calling thread (see
    // Core/Async.h), and it genuinely does hop off the UI thread today via
    // winrt::resume_after().
    winrt::fire_and_forget HomePage::PlayButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        auto queue = DispatcherQueue();

        auto cfg = Core::RealmConfig::Load();
        if (cfg.WowPath.empty())
        {
            StatusTextBlock().Text(L"No WoW install path configured - check Settings");
            StatusTextBlock().Visibility(Visibility::Visible);
            co_return;
        }

        bool launched = Core::WowInstall::LaunchWow(cfg.WowPath, cfg.RealmAddress);
        if (!launched)
        {
            StatusTextBlock().Text(L"Failed to launch WoW - check your install path in Settings");
            StatusTextBlock().Visibility(Visibility::Visible);
            co_return;
        }

        StatusTextBlock().Visibility(Visibility::Collapsed);

        if (cfg.CredentialVaultEnabled)
        {
            bool signedIn = co_await Core::CredentialVault::AutofillLoginAsync();

            queue.TryEnqueue([this, lifetime, signedIn]()
                {
                    if (!signedIn)
                    {
                        StatusTextBlock().Text(L"Not signed in - credentials not saved");
                        StatusTextBlock().Visibility(Visibility::Visible);
                    }
                });
        }
    }
}
