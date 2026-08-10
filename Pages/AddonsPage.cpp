#include "pch.h"
#include "AddonsPage.h"
#if __has_include("Pages/AddonsPage.g.cpp")
#include "Pages/AddonsPage.g.cpp"
#endif
#include "../Core/RealmConfig.h"
#include "../Core/AddonInstaller.h"
#include <winrt/Windows.System.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::AzerothCore::Pages::implementation
{
    namespace
    {
        Media::SolidColorBrush Brush(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
        {
            return Media::SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(a, r, g, b));
        }
    }

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
        {
            StatusTextBlock().Text(L"Set your WoW install path first");
            StatusTextBlock().Visibility(Visibility::Visible);
            return;
        }

        auto addonsDir = Core::AddonInstaller::ResolveAddOnsDir(cfg.WowPath);
        std::filesystem::create_directories(addonsDir);
        winrt::Windows::System::Launcher::LaunchFolderPathAsync(winrt::hstring(addonsDir.wstring()));
    }

    UIElement AddonsPage::BuildResultRow(Core::RemoteAddon const& addon)
    {
        auto cfg = Core::RealmConfig::Load();
        bool installed = !addon.AddonFolderName.empty()
                      && Core::AddonInstaller::IsInstalled(cfg.WowPath, addon.AddonFolderName);

        Grid row;
        row.ColumnSpacing(10);
        ColumnDefinition stretch; stretch.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        ColumnDefinition autoCol; autoCol.Width(GridLengthHelper::Auto());
        row.ColumnDefinitions().Append(stretch);
        row.ColumnDefinitions().Append(autoCol);

        StackPanel text;
        text.VerticalAlignment(VerticalAlignment::Center);

        TextBlock name;
        name.Text(addon.Name);
        name.Foreground(Brush(0xFF, 0xDC, 0xE4, 0xF2));
        name.FontSize(13);
        name.TextTrimming(TextTrimming::CharacterEllipsis);
        text.Children().Append(name);

        TextBlock meta;
        meta.Text(addon.SourceName + (addon.Author.empty() ? L"" : (L"  -  " + addon.Author)));
        meta.Foreground(Brush(0x99, 0xDC, 0xE4, 0xF2));
        meta.FontSize(11);
        text.Children().Append(meta);

        // Per-addon status. Starts out saying whether it is already present, so
        // the list is honest about what the client already has before anyone
        // clicks anything.
        TextBlock status;
        status.FontSize(11);
        status.TextWrapping(TextWrapping::Wrap);
        if (installed)
        {
            status.Text(L"Installed");
            status.Foreground(Brush(0xFF, 0x9E, 0xD1, 0x9E));
        }
        else
        {
            status.Foreground(Brush(0xFF, 0xCB, 0xB9, 0x8A));
        }
        text.Children().Append(status);

        Button install;
        install.Content(box_value(installed ? L"Reinstall" : L"Install"));
        install.Padding({ 14, 6, 14, 6 });
        install.CornerRadius({ 3, 3, 3, 3 });
        install.VerticalAlignment(VerticalAlignment::Center);
        install.Resources().Insert(box_value(L"ButtonBackground"), Brush(0x00, 0, 0, 0));
        install.Resources().Insert(box_value(L"ButtonBackgroundPointerOver"), Brush(0x33, 0xCB, 0xB9, 0x8A));
        install.Resources().Insert(box_value(L"ButtonBackgroundPressed"), Brush(0x59, 0xCB, 0xB9, 0x8A));
        install.Resources().Insert(box_value(L"ButtonForeground"), Brush(0xFF, 0xCB, 0xB9, 0x8A));
        install.Resources().Insert(box_value(L"ButtonForegroundPointerOver"), Brush(0xFF, 0xF0, 0xC8, 0x60));
        install.Resources().Insert(box_value(L"ButtonForegroundPressed"), Brush(0xFF, 0xF0, 0xC8, 0x60));
        install.Resources().Insert(box_value(L"ButtonBorderBrush"), Brush(0x66, 0xCB, 0xB9, 0x8A));
        install.Resources().Insert(box_value(L"ButtonBorderBrushPointerOver"), Brush(0xFF, 0xF0, 0xC8, 0x60));

        install.Click([this, addon, install, status](auto&&, auto&&)
            {
                InstallAddonAsync(addon, install, status);
            });

        Grid::SetColumn(text, 0);
        Grid::SetColumn(install, 1);
        row.Children().Append(text);
        row.Children().Append(install);

        Border wrapper;
        wrapper.Child(row);
        wrapper.Padding({ 10, 8, 10, 8 });
        wrapper.Margin({ 0, 0, 0, 6 });
        wrapper.CornerRadius({ 3, 3, 3, 3 });
        wrapper.Background(Brush(0x66, 0x14, 0x14, 0x14));
        wrapper.BorderBrush(Brush(0x33, 0xCB, 0xB9, 0x8A));
        wrapper.BorderThickness({ 1, 1, 1, 1 });
        return wrapper;
    }

    // Installs one addon and reports into that row's own status line. The
    // button is disabled for the duration so a second click cannot start a
    // parallel install of the same folder.
    //
    // Core::Task<T> does not preserve the calling thread (see Core/Async.h), so
    // DispatcherQueue() is captured here, before the co_await, while still on
    // the UI thread.
    winrt::fire_and_forget AddonsPage::InstallAddonAsync(
        Core::RemoteAddon addon, Button button, TextBlock status)
    {
        auto lifetime = get_strong();
        auto queue = DispatcherQueue();

        button.IsEnabled(false);
        status.Text(L"Installing...");
        status.Foreground(Brush(0xFF, 0xCB, 0xB9, 0x8A));

        auto cfg = Core::RealmConfig::Load();
        auto result = co_await Core::AddonInstaller::InstallAsync(addon, cfg.WowPath);

        queue.TryEnqueue([this, lifetime, button, status, result]()
            {
                bool ok = result.Status == Core::InstallStatus::Installed
                       || result.Status == Core::InstallStatus::AlreadyInstalled;

                // Report the real reason rather than a generic failure. The
                // installer distinguishes a missing client path from a bad
                // download from an unsafe archive, and each needs a different
                // response from the user.
                status.Text(result.Message);
                status.Foreground(ok ? Brush(0xFF, 0x9E, 0xD1, 0x9E)
                                     : Brush(0xFF, 0xFF, 0x9B, 0x9B));

                button.IsEnabled(true);
                if (ok)
                    button.Content(box_value(L"Reinstall"));
            });
    }

    // Pattern to copy for any Core::Task<T>-returning call from a UI event
    // handler: Task<T> does NOT preserve the calling thread/apartment (see
    // Core/Async.h). m_catalog.SearchAsync hops onto a background thread, so
    // DispatcherQueue() -- itself a property of this thread-affine
    // DependencyObject -- MUST be captured before the co_await.
    winrt::fire_and_forget AddonsPage::RunSearchAsync(std::wstring query)
    {
        auto lifetime = get_strong();
        auto queue = DispatcherQueue();
        auto result = co_await m_catalog.SearchAsync(query);

        queue.TryEnqueue([this, lifetime, result]()
            {
                ResultsList().Items().Clear();
                for (auto const& addon : result.Addons)
                    ResultsList().Items().Append(BuildResultRow(addon));

                if (result.AnySourceFailed && result.Addons.empty())
                {
                    StatusTextBlock().Text(L"Addon search unavailable");
                    StatusTextBlock().Visibility(Visibility::Visible);
                }
                else if (result.Addons.empty())
                {
                    StatusTextBlock().Text(L"No addons matched");
                    StatusTextBlock().Visibility(Visibility::Visible);
                }
                else
                {
                    StatusTextBlock().Visibility(Visibility::Collapsed);
                }
            });
    }
}
