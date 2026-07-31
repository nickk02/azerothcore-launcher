#include "pch.h"
#include "CharactersPage.h"
#if __has_include("Pages/CharactersPage.g.cpp")
#include "Pages/CharactersPage.g.cpp"
#endif
#include "../Core/ArmoryClient.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::AzerothCore::Pages::implementation
{
    CharactersPage::CharactersPage()
    {
        InitializeComponent();
        LoadCharactersAsync();
    }

    // See the DispatcherQueue()-hoisting note on AddonsPage::RunSearchAsync
    // (Pages/AddonsPage.cpp) for why `queue` is captured before the
    // co_await rather than calling DispatcherQueue() after it: Core::Task<T>
    // does not preserve the calling thread, so DispatcherQueue() -- itself a
    // thread-affine property -- must be read while still on the UI thread.
    // ArmoryClient::FetchCharactersAsync happens not to suspend today, so
    // this stays on the UI thread either way, but that stops being true the
    // moment a real backend replaces it.
    winrt::fire_and_forget CharactersPage::LoadCharactersAsync()
    {
        auto lifetime = get_strong();
        auto queue = DispatcherQueue();
        auto characters = co_await Core::ArmoryClient::FetchCharactersAsync(L"");

        queue.TryEnqueue([this, lifetime, characters]()
            {
                if (characters.empty())
                {
                    EmptyStateText().Visibility(Visibility::Visible);
                    CharacterList().Visibility(Visibility::Collapsed);
                    return;
                }

                EmptyStateText().Visibility(Visibility::Collapsed);
                CharacterList().Visibility(Visibility::Visible);
                CharacterList().Items().Clear();
                for (auto const& c : characters)
                {
                    TextBlock item;
                    item.Text(c.Name + L" - Level " + std::to_wstring(c.Level) + L" " + c.Race + L" " + c.Class);
                    item.Foreground(Media::SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xDC, 0xE4, 0xF2)));
                    CharacterList().Items().Append(item);
                }
            });
    }
}
