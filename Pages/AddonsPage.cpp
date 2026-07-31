#include "pch.h"
#include "AddonsPage.h"
#if __has_include("Pages/AddonsPage.g.cpp")
#include "Pages/AddonsPage.g.cpp"
#endif
#include "../Core/RealmConfig.h"
#include <winrt/Windows.System.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::AzerothCore::Pages::implementation
{
    AddonsPage::AddonsPage()
    {
        InitializeComponent();
        RunSearchAsync(L"");
    }

    void AddonsPage::SearchButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        RunSearchAsync(SearchBox().Text().c_str());
    }

    void AddonsPage::SearchBox_KeyDown(IInspectable const&, Input::KeyRoutedEventArgs const& e)
    {
        if (e.Key() == winrt::Windows::System::VirtualKey::Enter)
            RunSearchAsync(SearchBox().Text().c_str());
    }

    void AddonsPage::OpenFolderLink_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto cfg = Core::RealmConfig::Load();
        if (cfg.WowPath.empty())
            return;

        std::filesystem::path addonsDir = std::filesystem::path(cfg.WowPath).parent_path() / L"Interface" / L"AddOns";
        std::filesystem::create_directories(addonsDir);
        winrt::Windows::System::Launcher::LaunchFolderPathAsync(winrt::hstring(addonsDir.wstring()));
    }

    // Pattern to copy for any future Core::Task<T>-returning call from a UI
    // event handler: Task<T> does NOT preserve the calling thread/apartment
    // (see Core/Async.h) -- it may resume on whatever thread the coroutine
    // body finishes on. m_catalog.SearchAsync hops onto a background thread
    // (FelbiteSource::SearchAsync calls resume_background()), so
    // DispatcherQueue() -- itself a property of this thread-affine
    // DependencyObject -- MUST be captured here, before the co_await, while
    // still on the UI thread. Calling DispatcherQueue() AFTER the co_await
    // would be reading a thread-affine property from a background thread.
    winrt::fire_and_forget AddonsPage::RunSearchAsync(std::wstring query)
    {
        auto lifetime = get_strong();
        auto queue = DispatcherQueue();
        auto result = co_await m_catalog.SearchAsync(query);

        queue.TryEnqueue([this, lifetime, result]()
            {
                ResultsList().Items().Clear();
                for (auto const& addon : result.Addons)
                {
                    TextBlock item;
                    item.Text(addon.Name + L"  -  " + addon.SourceName);
                    item.Foreground(Media::SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xDC, 0xE4, 0xF2)));
                    item.Margin({ 0, 4, 0, 4 });
                    ResultsList().Items().Append(item);
                }

                if (result.AnySourceFailed && result.Addons.empty())
                {
                    StatusTextBlock().Text(L"Addon search unavailable");
                    StatusTextBlock().Visibility(Visibility::Visible);
                }
                else
                {
                    StatusTextBlock().Visibility(Visibility::Collapsed);
                }
            });
    }
}
