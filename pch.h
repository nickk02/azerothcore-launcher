#pragma once

#include <unknwn.h>

// windows.h (pulled in by unknwn.h) defines GetCurrentTime as a macro for
// GetTickCount, and the generated Animation projection declares
// Timeline::GetCurrentTime(int64_t*). With the macro live, that declaration is
// rewritten and the header fails to compile, pointing into generated code with
// "'result': identifier not found".
//
// This MUST sit above every winrt include, not just above the Animation one.
// Microsoft.UI.Xaml.h transitively pulls in the Animation forward-declaration
// header, so undefining later leaves the declaration half parsed with the macro
// applied and the definition half without it -- which fails differently, as
// "'GetCurrentTime': is not a member of consume_...IStoryboard".
//
// Nothing in this project calls the Win32 GetCurrentTime, so dropping the macro
// costs nothing.
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

// Animation and Shapes are separate namespace headers from Media -- omitting
// them gives C3779 "function that returns 'auto' cannot be used before it is
// defined" on every Storyboard/DoubleAnimation/Rectangle call, which reads like
// a language error but is just a missing include.
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <microsoft.ui.xaml.window.h>
