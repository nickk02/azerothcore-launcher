#pragma once
#include "MainWindow.g.h"

namespace winrt::AzerothCore::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

    private:
        void SetupCustomTitleBar();
        void SetupFixedWindow();
    };
}

namespace winrt::AzerothCore::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {};
}
