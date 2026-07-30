#pragma once

#include "App.xaml.g.h"

namespace winrt::AzerothCore::implementation
{
    struct App : AppT<App>
    {
        App();
        void OnLaunched();

    private:
        void* m_window{ nullptr };
    };
}
