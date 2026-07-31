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

    winrt::fire_and_forget AddonsPage::RunSearchAsync(std::wstring query)
    {
        auto lifetime = get_strong();
        auto results = co_await m_catalog.SearchAsync(query);

        DispatcherQueue().TryEnqueue([this, lifetime, results]()
            {
                ResultsList().Items().Clear();
                for (auto const& addon : results)
                {
                    TextBlock item;
                    item.Text(addon.Name + L"  —  " + addon.SourceName);
                    item.Foreground(Media::SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xDC, 0xE4, 0xF2)));
                    item.Margin({ 0, 4, 0, 4 });
                    ResultsList().Items().Append(item);
                }
            });
    }
}
