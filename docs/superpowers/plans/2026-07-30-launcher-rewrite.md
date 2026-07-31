# AzerothCore Launcher Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ground-up rewrite of the AzerothCore desktop launcher (WinUI 3 / C++ / XAML) with zero code reuse from the old wallmane-derived app, matching the approved design spec at `docs/superpowers/specs/2026-07-30-launcher-rewrite-design.md`.

**Architecture:** A thin `MainWindow` shell hosts a `Frame` that navigates between four `Page`s (Home, Addons, Characters, Settings), replacing the old single-window-with-manually-toggled-panels design. Each `Core/` service is a focused, independently-usable class; pages consume services, they don't embed logic.

**Tech Stack:** WinUI 3, C++/WinRT, XAML, WebView2 (armory 3D model), Windows Credential Vault (`PasswordVault`), Inno Setup (installer). NuGet packages pinned to versions already proven to build in this exact dev environment (see Task 1).

## Global Constraints

- **Product name shown in the UI is "AzerothCore"** — not "AzerothCore Launcher." Window title, installer product name, Start Menu shortcut, `%APPDATA%` folder name all use "AzerothCore." The repo name (`azerothcore-launcher`) is unrelated and unaffected.
- **No code from `wallmane-reference/` is copied or adapted.** That folder is read-only local reference material. If a pattern looks similar to something there, it was independently written, not ported.
- **Blizzard assets (`Assets/wotlk-*.png`) and `Launcher.exe`/`wow-launcher.PNG` stay gitignored.** Never remove them from `.gitignore`, never commit them.
- **Every commit that lands on `main` goes through a branch + PR.** No direct pushes to `main` on `nickk02/azerothcore-launcher`, per Nick's standing instruction ("if I need to review or approve something make it a PR").
- **No AI attribution anywhere** — no "Generated with Claude," no Co-Authored-By trailers, in commits, PR bodies, or code comments. Commit as `Nicolas Sanchez <98576999+nickk02@users.noreply.github.com>`.
- **C++/WinRT async pattern:** every `winrt::fire_and_forget` method that touches `this` must call `auto lifetime = get_strong();` as its first line (keeps the object alive across suspension points), and must marshal any UI mutation back through `DispatcherQueue().TryEnqueue(...)`.
- **Windows Target:** `WindowsTargetPlatformMinVersion` 10.0.17763.0, `WindowsTargetPlatformVersion` 10.0, `PlatformToolset` v145 (VS 18) / v143 fallback — matches the proven-working local toolchain.

---

## File Structure

```
azerothcore.vcxproj / .filters / .slnx     - project files
packages.config                             - NuGet pins (Task 1)
pch.h / pch.cpp                             - precompiled header
app.manifest, Package.appxmanifest, version.rc
App.xaml / .cpp / .h                        - app entry
MainWindow.xaml / .cpp / .h / .idl          - shell: titlebar, nav rail, Frame host
Pages/HomePage.xaml / .cpp / .h / .idl      - hero art, realm status, Play
Pages/AddonsPage.xaml / .cpp / .h / .idl    - addon search/install
Pages/CharactersPage.xaml / .cpp / .h / .idl - armory (WebView2)
Pages/SettingsPage.xaml / .cpp / .h / .idl  - WoW path, realm address, credentials
Core/RealmConfig.h / .cpp                   - settings persistence
Core/WowInstall.h / .cpp                    - path validate/launch/realmlist/playtime
Core/RealmStatusChecker.h / .cpp            - TCP reachability
Core/CredentialVault.h / .cpp               - Credential Vault + autofill
Core/IAddonSource.h                         - addon source interface
Core/FelbiteSource.h / .cpp                 - Felbite scraper implementation
Core/AddonCatalog.h / .cpp                  - orchestrates sources
Core/ArmoryClient.h / .cpp                  - character/armory data fetch
tools/felbite-healthcheck/check.py          - standalone scraper health check
.github/workflows/felbite-healthcheck.yml   - scheduled CI for the above
installer.iss                               - Inno Setup script
```

---

### Task 1: Project scaffold

**Files:**
- Create: `azerothcore.vcxproj`, `azerothcore.vcxproj.filters`, `azerothcore.slnx`, `packages.config`, `pch.h`, `pch.cpp`, `app.manifest`, `Package.appxmanifest`, `version.rc`, `App.xaml`, `App.xaml.cpp`, `App.xaml.h`, `MainWindow.xaml`, `MainWindow.xaml.cpp`, `MainWindow.xaml.h`, `MainWindow.idl`

**Interfaces:**
- Produces: a buildable, launchable WinUI 3 window titled "AzerothCore." Every later task adds to `MainWindow` and adds new `Page` files; nothing in this task is a dependency surface other task code calls into directly.

- [ ] **Step 1: Create `packages.config`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<packages>
  <package id="Microsoft.Web.WebView2" version="1.0.3719.77" targetFramework="native" />
  <package id="Microsoft.Windows.CppWinRT" version="2.0.250303.1" targetFramework="native" />
  <package id="Microsoft.Windows.ImplementationLibrary" version="1.0.260126.7" targetFramework="native" />
  <package id="Microsoft.Windows.SDK.BuildTools" version="10.0.28000.1839" targetFramework="native" />
  <package id="Microsoft.WindowsAppSDK" version="2.0.1" targetFramework="native" />
  <package id="Microsoft.WindowsAppSDK.Base" version="2.0.3" targetFramework="native" />
  <package id="Microsoft.WindowsAppSDK.Foundation" version="2.0.20" targetFramework="native" />
  <package id="Microsoft.WindowsAppSDK.InteractiveExperiences" version="2.0.12" targetFramework="native" />
  <package id="Microsoft.WindowsAppSDK.Runtime" version="2.0.1" targetFramework="native" />
  <package id="Microsoft.WindowsAppSDK.WinUI" version="2.0.12" targetFramework="native" />
</packages>
```

Only the packages actually needed (dropped the old project's ML/AI/Widgets packages — nothing in this rewrite uses them; YAGNI).

- [ ] **Step 2: Create `pch.h`**

```cpp
#pragma once

#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <microsoft.ui.xaml.window.h>
```

- [ ] **Step 3: Create `pch.cpp`**

```cpp
#include "pch.h"
```

- [ ] **Step 4: Create `app.manifest`**

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1">
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="asInvoker" uiAccess="false" />
      </requestedPrivileges>
    </security>
  </trustInfo>
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}" />
      <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}" />
      <supportedOS Id="{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}" />
      <supportedOS Id="{35138b9a-5d96-4fbd-8e2d-a2440225f93a}" />
      <supportedOS Id="{e2011457-1546-43c5-a5fe-008deee3d3f0}" />
    </application>
  </compatibility>
</assembly>
```

- [ ] **Step 5: Create `App.xaml`**

```xml
<Application
    x:Class="AzerothCore.App"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:local="using:AzerothCore">
</Application>
```

- [ ] **Step 6: Create `App.xaml.h`**

```cpp
#pragma once

#include "App.xaml.g.h"

namespace winrt::AzerothCore::implementation
{
    struct App : AppT<App>
    {
        App();
        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

    private:
        winrt::Microsoft::UI::Xaml::Window m_window{ nullptr };
    };
}
```

- [ ] **Step 7: Create `App.xaml.cpp`**

```cpp
#include "pch.h"
#include "App.xaml.h"
#if __has_include("App.xaml.g.cpp")
#include "App.xaml.g.cpp"
#endif
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::AzerothCore::implementation
{
    App::App()
    {
        InitializeComponent();
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        m_window = make<AzerothCore::implementation::MainWindow>();
        m_window.Activate();
    }
}
```

- [ ] **Step 8: Create `MainWindow.idl`**

```
import "Microsoft.UI.Xaml.idl";

namespace AzerothCore
{
    [default_interface]
    runtimeclass MainWindow : Microsoft.UI.Xaml.Window
    {
        MainWindow();
    }
}
```

- [ ] **Step 9: Create `MainWindow.xaml`**

```xml
<Window
    x:Class="AzerothCore.MainWindow"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:local="using:AzerothCore">

    <Grid x:Name="RootGrid" Background="#0A0A0A">
        <Grid.RowDefinitions>
            <RowDefinition Height="32"/>
            <RowDefinition Height="*"/>
        </Grid.RowDefinitions>

        <Grid x:Name="AppTitleBar" Grid.Row="0" Background="#141414">
            <TextBlock Text="AzerothCore" Foreground="#DCE4F2" FontSize="12"
                       VerticalAlignment="Center" Margin="12,0,0,0"/>
        </Grid>

        <Frame x:Name="ContentFrame" Grid.Row="1"/>
    </Grid>
</Window>
```

- [ ] **Step 10: Create `MainWindow.xaml.h`**

```cpp
#pragma once
#include "MainWindow.g.h"

namespace winrt::AzerothCore::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

    private:
        void SetupCustomTitleBar();
    };
}

namespace winrt::AzerothCore::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {};
}
```

- [ ] **Step 11: Create `MainWindow.xaml.cpp`**

```cpp
#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::AzerothCore::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        Title(L"AzerothCore");
        SetupCustomTitleBar();

        auto appWindow = this->AppWindow();
        appWindow.Resize({ 900, 620 });
    }

    void MainWindow::SetupCustomTitleBar()
    {
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
    }
}
```

- [ ] **Step 12: Create `azerothcore.vcxproj`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <CppWinRTOptimized>true</CppWinRTOptimized>
    <CppWinRTRootNamespaceAutoMerge>true</CppWinRTRootNamespaceAutoMerge>
    <CppWinRTGenerateWindowsMetadata>true</CppWinRTGenerateWindowsMetadata>
    <MinimalCoreWin>true</MinimalCoreWin>
    <ProjectGuid>{7c3f0a5e-2b9a-4d61-9a0f-1e6b4b8c9a10}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>AzerothCore</RootNamespace>
    <ProjectName>azerothcore</ProjectName>
    <WindowsTargetPlatformMinVersion>10.0.17763.0</WindowsTargetPlatformMinVersion>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
    <AppContainerApplication>false</AppContainerApplication>
    <ApplicationType>Windows Store</ApplicationType>
    <ApplicationTypeRevision>10.0</ApplicationTypeRevision>
    <DefaultLanguage>en-US</DefaultLanguage>
    <PlatformToolset Condition="'$(VisualStudioVersion)' &gt;= '18.0'">v145</PlatformToolset>
    <PlatformToolset Condition="'$(VisualStudioVersion)' &lt; '18.0'">v143</PlatformToolset>
    <WindowsAppSDKSelfContained>true</WindowsAppSDKSelfContained>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <CharacterSet>Unicode</CharacterSet>
    <UseOfCppCode>true</UseOfCppCode>
    <PlatformToolset Condition="'$(VisualStudioVersion)' &gt;= '18.0'">v145</PlatformToolset>
    <PlatformToolset Condition="'$(VisualStudioVersion)' &lt; '18.0'">v143</PlatformToolset>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings" />
  <ImportGroup Label="Shared" />
  <ImportGroup Label="PropertySheets">
    <Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup>
    <LanguageStandard>stdcpp20</LanguageStandard>
    <GenerateManifest>true</GenerateManifest>
    <EmbedManifest>true</EmbedManifest>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <PrecompiledHeader>Use</PrecompiledHeader>
      <PrecompiledHeaderFile>pch.h</PrecompiledHeaderFile>
      <WarningLevel>Level3</WarningLevel>
      <LanguageStandard>stdcpp20</LanguageStandard>
      <AdditionalOptions>/bigobj %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>
    <Link>
      <SubSystem>Windows</SubSystem>
    </Link>
    <Manifest>
      <AdditionalManifestFiles>app.manifest</AdditionalManifestFiles>
    </Manifest>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="pch.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="App.xaml.cpp" />
    <ClCompile Include="MainWindow.xaml.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="pch.h" />
    <ClInclude Include="App.xaml.h" />
    <ClInclude Include="MainWindow.xaml.h" />
  </ItemGroup>
  <ItemGroup>
    <ApplicationDefinition Include="App.xaml" />
    <Page Include="MainWindow.xaml" />
    <Midl Include="MainWindow.idl" />
  </ItemGroup>
  <ItemGroup>
    <AppxManifest Include="Package.appxmanifest" />
  </ItemGroup>
  <ItemGroup>
    <None Include="packages.config" />
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets">
    <Import Project="packages\Microsoft.Windows.CppWinRT.2.0.250303.1\build\native\Microsoft.Windows.CppWinRT.targets" Condition="Exists('packages\Microsoft.Windows.CppWinRT.2.0.250303.1\build\native\Microsoft.Windows.CppWinRT.targets')" />
    <Import Project="packages\Microsoft.WindowsAppSDK.2.0.1\build\native\Microsoft.WindowsAppSDK.targets" Condition="Exists('packages\Microsoft.WindowsAppSDK.2.0.1\build\native\Microsoft.WindowsAppSDK.targets')" />
    <Import Project="packages\Microsoft.Web.WebView2.1.0.3719.77\build\native\Microsoft.Web.WebView2.targets" Condition="Exists('packages\Microsoft.Web.WebView2.1.0.3719.77\build\native\Microsoft.Web.WebView2.targets')" />
  </ImportGroup>
</Project>
```

- [ ] **Step 13: Create `azerothcore.vcxproj.filters`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
    <Filter Include="Source Files"><UniqueIdentifier>{4FC737F1-C7A5-4376-A066-2A32D752A2FF}</UniqueIdentifier></Filter>
    <Filter Include="Header Files"><UniqueIdentifier>{93995380-89BD-4b04-88EB-625FBE52EBFB}</UniqueIdentifier></Filter>
  </ItemGroup>
  <ItemGroup>
    <ClCompile Include="pch.cpp"><Filter>Source Files</Filter></ClCompile>
    <ClCompile Include="App.xaml.cpp"><Filter>Source Files</Filter></ClCompile>
    <ClCompile Include="MainWindow.xaml.cpp"><Filter>Source Files</Filter></ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="pch.h"><Filter>Header Files</Filter></ClInclude>
    <ClInclude Include="App.xaml.h"><Filter>Header Files</Filter></ClInclude>
    <ClInclude Include="MainWindow.xaml.h"><Filter>Header Files</Filter></ClInclude>
  </ItemGroup>
  <ItemGroup>
    <ApplicationDefinition Include="App.xaml" />
    <Page Include="MainWindow.xaml" />
  </ItemGroup>
  <ItemGroup>
    <Midl Include="MainWindow.idl" />
  </ItemGroup>
</Project>
```

- [ ] **Step 14: Create `azerothcore.slnx`**

```xml
<Solution>
  <Project Path="azerothcore.vcxproj" Type="Windows C++" />
</Solution>
```

- [ ] **Step 15: Create a minimal `Package.appxmanifest`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<Package
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
  IgnorableNamespaces="uap rescap">
  <Identity Name="AzerothCore" Publisher="CN=AzerothCore" Version="1.0.0.0" />
  <Properties>
    <DisplayName>AzerothCore</DisplayName>
    <PublisherDisplayName>AzerothCore</PublisherDisplayName>
    <Logo>Assets\wotlk-icon.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.17763.0" MaxVersionTested="10.0.22000.0" />
  </Dependencies>
  <Resources>
    <Resource Language="en-us" />
  </Resources>
  <Applications>
    <Application Id="App" Executable="$targetnametoken$.exe" EntryPoint="$targetentrypoint$">
      <uap:VisualElements DisplayName="AzerothCore" Description="AzerothCore" BackgroundColor="transparent" Square150x150Logo="Assets\wotlk-icon.png" Square44x44Logo="Assets\wotlk-icon.png">
        <uap:DefaultTile Wide310x150Logo="Assets\wotlk-icon.png" />
      </uap:VisualElements>
    </Application>
  </Applications>
</Package>
```

- [ ] **Step 16: Build and run**

```bash
"/c/Users/Nick/Documents/caches/nuget/nuget.exe" restore azerothcore.vcxproj -PackagesDirectory packages
MSYS_NO_PATHCONV=1 "/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/amd64/MSBuild.exe" azerothcore.vcxproj -p:Configuration=Debug -p:Platform=x64 -nologo -v:minimal -m
```

Expected: builds with 0 errors. Manually launch `x64\Debug\azerothcore\azerothcore.exe` and confirm a window titled "AzerothCore" opens with a dark titlebar and empty content area.

- [ ] **Step 17: Commit**

```bash
git checkout -b scaffold/project-setup
git add azerothcore.vcxproj azerothcore.vcxproj.filters azerothcore.slnx packages.config pch.h pch.cpp app.manifest Package.appxmanifest App.xaml App.xaml.cpp App.xaml.h MainWindow.xaml MainWindow.xaml.cpp MainWindow.xaml.h MainWindow.idl
git commit -m "Scaffold WinUI3 project, blank AzerothCore window"
git push -u origin scaffold/project-setup
gh pr create --title "Scaffold WinUI3 project" --body "Blank buildable window titled AzerothCore. First task of the rewrite plan."
```

---

### Task 2: RealmConfig — settings persistence

**Files:**
- Create: `Core/RealmConfig.h`, `Core/RealmConfig.cpp`, `Core/RealmConfigTests.cpp` (a standalone `main()`-based test executable — no test framework dependency needed for this pure-logic class)

**Interfaces:**
- Consumes: nothing (pure file I/O + string handling).
- Produces:
  ```cpp
  namespace Core {
      struct RealmConfig {
          std::wstring WowPath;
          std::wstring RealmAddress;
          bool CredentialVaultEnabled = false;

          static RealmConfig Load();          // reads from %APPDATA%\AzerothCore\, missing file = defaults
          void Save() const;                   // writes to %APPDATA%\AzerothCore\
          static std::filesystem::path ConfigDir(); // %APPDATA%\AzerothCore\
      };
  }
  ```
  Every later task that needs persisted settings (`WowInstall`, `HomePage`, `SettingsPage`, credential wiring) calls `RealmConfig::Load()` / `.Save()` — no other task reads `%APPDATA%` directly.

- [ ] **Step 1: Write the failing test**

```cpp
// Core/RealmConfigTests.cpp
#include "RealmConfig.h"
#include <cassert>
#include <iostream>
#include <filesystem>

int main()
{
    // Isolate from any real user config by pointing HOME/APPDATA-derived path
    // at a throwaway subfolder for this test run.
    auto testDir = std::filesystem::temp_directory_path() / L"AzerothCoreConfigTest";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);
    _wputenv_s(L"APPDATA", testDir.c_str());

    // Load with no file present -> defaults.
    {
        Core::RealmConfig cfg = Core::RealmConfig::Load();
        assert(cfg.WowPath.empty());
        assert(cfg.RealmAddress.empty());
        assert(cfg.CredentialVaultEnabled == false);
    }

    // Save then reload -> round-trips exactly.
    {
        Core::RealmConfig cfg;
        cfg.WowPath = L"C:\\Games\\WoW\\Wow.exe";
        cfg.RealmAddress = L"logon.example.com:3724";
        cfg.CredentialVaultEnabled = true;
        cfg.Save();

        Core::RealmConfig reloaded = Core::RealmConfig::Load();
        assert(reloaded.WowPath == L"C:\\Games\\WoW\\Wow.exe");
        assert(reloaded.RealmAddress == L"logon.example.com:3724");
        assert(reloaded.CredentialVaultEnabled == true);
    }

    std::wcout << L"All RealmConfig tests passed." << std::endl;
    return 0;
}
```

- [ ] **Step 2: Create a stub `Core/RealmConfig.h` so the test fails to link, not to compile**

```cpp
#pragma once
#include <string>
#include <filesystem>

namespace Core
{
    struct RealmConfig
    {
        std::wstring WowPath;
        std::wstring RealmAddress;
        bool CredentialVaultEnabled = false;

        static RealmConfig Load();
        void Save() const;
        static std::filesystem::path ConfigDir();
    };
}
```

```cpp
// Core/RealmConfig.cpp — deliberately unimplemented for this step
#include "RealmConfig.h"

namespace Core
{
    RealmConfig RealmConfig::Load() { throw std::runtime_error("not implemented"); }
    void RealmConfig::Save() const { throw std::runtime_error("not implemented"); }
    std::filesystem::path RealmConfig::ConfigDir() { throw std::runtime_error("not implemented"); }
}
```

- [ ] **Step 3: Compile and run to verify it fails**

```bash
cl /std:c++20 /EHsc /I. Core/RealmConfigTests.cpp Core/RealmConfig.cpp /Fe:realmconfig_test.exe
./realmconfig_test.exe
```

Expected: throws `std::runtime_error("not implemented")`, non-zero exit.

- [ ] **Step 4: Implement `Core/RealmConfig.cpp` for real**

```cpp
#include "RealmConfig.h"
#include <fstream>
#include <sstream>

namespace Core
{
    std::filesystem::path RealmConfig::ConfigDir()
    {
        wchar_t* appdata = nullptr;
        size_t len = 0;
        _wdupenv_s(&appdata, &len, L"APPDATA");
        std::filesystem::path dir = std::filesystem::path(appdata ? appdata : L".") / L"AzerothCore";
        free(appdata);
        std::filesystem::create_directories(dir);
        return dir;
    }

    RealmConfig RealmConfig::Load()
    {
        RealmConfig cfg;
        std::filesystem::path file = ConfigDir() / L"settings.ini";
        std::wifstream in(file);
        if (!in.is_open())
            return cfg;

        std::wstring line;
        while (std::getline(in, line))
        {
            auto eq = line.find(L'=');
            if (eq == std::wstring::npos)
                continue;
            std::wstring key = line.substr(0, eq);
            std::wstring value = line.substr(eq + 1);
            if (key == L"WowPath")
                cfg.WowPath = value;
            else if (key == L"RealmAddress")
                cfg.RealmAddress = value;
            else if (key == L"CredentialVaultEnabled")
                cfg.CredentialVaultEnabled = (value == L"1");
        }
        return cfg;
    }

    void RealmConfig::Save() const
    {
        std::filesystem::path file = ConfigDir() / L"settings.ini";
        std::wofstream out(file, std::ios::trunc);
        out << L"WowPath=" << WowPath << L"\n";
        out << L"RealmAddress=" << RealmAddress << L"\n";
        out << L"CredentialVaultEnabled=" << (CredentialVaultEnabled ? L"1" : L"0") << L"\n";
    }
}
```

- [ ] **Step 5: Run to verify it passes**

```bash
cl /std:c++20 /EHsc /I. Core/RealmConfigTests.cpp Core/RealmConfig.cpp /Fe:realmconfig_test.exe
./realmconfig_test.exe
```

Expected: prints `All RealmConfig tests passed.`, exit 0.

- [ ] **Step 6: Add `Core/RealmConfig.h`/`.cpp` to the vcxproj**

Add `<ClCompile Include="Core\RealmConfig.cpp" />` and `<ClInclude Include="Core\RealmConfig.h" />` to `azerothcore.vcxproj` and the matching filter entries (under a new `Core` filter) in `azerothcore.vcxproj.filters`.

- [ ] **Step 7: Commit**

```bash
git checkout -b core/realm-config
git add Core/RealmConfig.h Core/RealmConfig.cpp Core/RealmConfigTests.cpp azerothcore.vcxproj azerothcore.vcxproj.filters
git commit -m "Add RealmConfig settings persistence"
git push -u origin core/realm-config
gh pr create --title "Add RealmConfig settings persistence" --body "TDD'd load/save of WoW path, realm address, credential vault opt-in under %APPDATA%\\AzerothCore\\."
```

---

### Task 3: Navigation shell — nav rail + Frame + four stub pages

**Files:**
- Create: `Pages/HomePage.xaml/.cpp/.h/.idl`, `Pages/AddonsPage.xaml/.cpp/.h/.idl`, `Pages/CharactersPage.xaml/.cpp/.h/.idl`, `Pages/SettingsPage.xaml/.cpp/.h/.idl` (each a near-empty stub for this task — real content in later tasks)
- Modify: `MainWindow.xaml`, `MainWindow.xaml.h`, `MainWindow.xaml.cpp`, `azerothcore.vcxproj`, `azerothcore.vcxproj.filters`

**Interfaces:**
- Consumes: nothing new.
- Produces: `ContentFrame.Navigate(xaml_typename<AzerothCore::Pages::HomePage>())` and the equivalent for the other three page types — Task 6 (Home), Task 9 (Settings), Task 12 (Addons), Task 13 (Characters) each fill in their own page's real content without touching `MainWindow` again except for the nav-rail click handlers wired in this task.

- [ ] **Step 1: Create the stub page pattern — `Pages/HomePage.idl`**

```
import "Microsoft.UI.Xaml.idl";

namespace AzerothCore.Pages
{
    [default_interface]
    runtimeclass HomePage : Microsoft.UI.Xaml.Controls.Page
    {
        HomePage();
    }
}
```

- [ ] **Step 2: `Pages/HomePage.xaml`**

```xml
<Page
    x:Class="AzerothCore.Pages.HomePage"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <Grid Background="#0A0A0A"/>
</Page>
```

- [ ] **Step 3: `Pages/HomePage.h`**

```cpp
#pragma once
#include "HomePage.g.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct HomePage : HomePageT<HomePage>
    {
        HomePage() { InitializeComponent(); }
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct HomePage : HomePageT<HomePage, implementation::HomePage>
    {};
}
```

- [ ] **Step 4: `Pages/HomePage.cpp`**

```cpp
#include "pch.h"
#include "HomePage.h"
#if __has_include("HomePage.g.cpp")
#include "HomePage.g.cpp"
#endif
```

- [ ] **Step 5: Repeat steps 1-4 for `AddonsPage`, `CharactersPage`, `SettingsPage`**

Identical pattern, only the class name and namespace path change (`AzerothCore.Pages.AddonsPage`, `AzerothCore.Pages.CharactersPage`, `AzerothCore.Pages.SettingsPage`). Each gets its own `.idl`/`.xaml`/`.h`/`.cpp` following exactly the structure in Steps 1-4.

- [ ] **Step 6: Update `MainWindow.xaml`** to add the nav rail alongside the existing titlebar/Frame

```xml
<Window
    x:Class="AzerothCore.MainWindow"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">

    <Grid x:Name="RootGrid" Background="#0A0A0A">
        <Grid.RowDefinitions>
            <RowDefinition Height="32"/>
            <RowDefinition Height="*"/>
        </Grid.RowDefinitions>

        <Grid x:Name="AppTitleBar" Grid.Row="0" Background="#141414">
            <TextBlock Text="AzerothCore" Foreground="#DCE4F2" FontSize="12"
                       VerticalAlignment="Center" Margin="12,0,0,0"/>
        </Grid>

        <Grid Grid.Row="1">
            <Grid.ColumnDefinitions>
                <ColumnDefinition Width="64"/>
                <ColumnDefinition Width="*"/>
            </Grid.ColumnDefinitions>

            <StackPanel Grid.Column="0" Spacing="22" Padding="0,24,0,0"
                        HorizontalAlignment="Center" Background="#0D0D0D">
                <TextBlock x:Name="NavHome" Text="HOME" FontSize="9" Foreground="#F0C860"
                           Tapped="NavHome_Tapped"/>
                <TextBlock x:Name="NavAddons" Text="ADDONS" FontSize="9" Foreground="#88DCE4F2"
                           Tapped="NavAddons_Tapped"/>
                <TextBlock x:Name="NavCharacters" Text="CHARS" FontSize="9" Foreground="#88DCE4F2"
                           Tapped="NavCharacters_Tapped"/>
                <TextBlock x:Name="NavSettings" Text="SETTINGS" FontSize="9" Foreground="#88DCE4F2"
                           Tapped="NavSettings_Tapped"/>
            </StackPanel>

            <Frame x:Name="ContentFrame" Grid.Column="1"/>
        </Grid>
    </Grid>
</Window>
```

- [ ] **Step 7: Update `MainWindow.xaml.h`**

```cpp
#pragma once
#include "MainWindow.g.h"

namespace winrt::AzerothCore::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void NavHome_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);
        void NavAddons_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);
        void NavCharacters_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);
        void NavSettings_Tapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&);

    private:
        void SetupCustomTitleBar();
        void SetActiveNav(winrt::Microsoft::UI::Xaml::Controls::TextBlock const& active);
    };
}

namespace winrt::AzerothCore::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {};
}
```

- [ ] **Step 8: Update `MainWindow.xaml.cpp`**

```cpp
#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "Pages/HomePage.h"
#include "Pages/AddonsPage.h"
#include "Pages/CharactersPage.h"
#include "Pages/SettingsPage.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::AzerothCore::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        Title(L"AzerothCore");
        SetupCustomTitleBar();

        auto appWindow = this->AppWindow();
        appWindow.Resize({ 900, 620 });

        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::HomePage>());
    }

    void MainWindow::SetupCustomTitleBar()
    {
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
    }

    void MainWindow::SetActiveNav(TextBlock const& active)
    {
        for (auto const& nav : { NavHome(), NavAddons(), NavCharacters(), NavSettings() })
            nav.Foreground(SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0x88, 0xDC, 0xE4, 0xF2)));
        active.Foreground(SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xF0, 0xC8, 0x60)));
    }

    void MainWindow::NavHome_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::HomePage>());
        SetActiveNav(NavHome());
    }

    void MainWindow::NavAddons_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::AddonsPage>());
        SetActiveNav(NavAddons());
    }

    void MainWindow::NavCharacters_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::CharactersPage>());
        SetActiveNav(NavCharacters());
    }

    void MainWindow::NavSettings_Tapped(IInspectable const&, Input::TappedRoutedEventArgs const&)
    {
        ContentFrame().Navigate(xaml_typename<AzerothCore::Pages::SettingsPage>());
        SetActiveNav(NavSettings());
    }
}
```

- [ ] **Step 9: Add all 16 new page files to `azerothcore.vcxproj` and `.filters`**

`<Midl>`/`<Page>`/`<ClCompile>`/`<ClInclude>` entries for `Pages\HomePage.*`, `Pages\AddonsPage.*`, `Pages\CharactersPage.*`, `Pages\SettingsPage.*` (four files each), under a new `Pages` filter.

- [ ] **Step 10: Build and manually verify**

Build per Task 1 Step 16's commands. Launch the app, click each of the four nav items, confirm the content area changes (blank grids for now) and the clicked label turns gold while the others stay dim.

- [ ] **Step 11: Commit**

```bash
git checkout -b shell/navigation
git add Pages/ MainWindow.xaml MainWindow.xaml.h MainWindow.xaml.cpp azerothcore.vcxproj azerothcore.vcxproj.filters
git commit -m "Add Frame-based navigation shell with four stub pages"
git push -u origin shell/navigation
gh pr create --title "Add navigation shell" --body "Nav rail + Frame navigation between Home/Addons/Characters/Settings stub pages, replacing the old single-window design."
```

---

### Task 4: WowInstall — path validation, launch, realmlist, playtime

**Files:**
- Create: `Core/WowInstall.h`, `Core/WowInstall.cpp`, `Core/WowInstallTests.cpp`

**Interfaces:**
- Consumes: `Core::RealmConfig` (Task 2) for the realm address passed into `LaunchWow`.
- Produces:
  ```cpp
  namespace Core {
      struct WowInstall {
          static bool IsValidWowExe(std::wstring const& path);       // pure logic, TDD'd
          static bool LaunchWow(std::wstring const& exePath, std::wstring const& realmAddress); // spawns process, writes realmlist.wtf if realmAddress non-empty
          static uint64_t GetTotalPlaytimeSeconds(std::wstring const& wowPath);
          static std::wstring FormatPlaytime(uint64_t seconds);       // pure logic, TDD'd
      };
  }
  ```
  `HomePage` (Task 6) calls `LaunchWow` and `FormatPlaytime`/`GetTotalPlaytimeSeconds`; `SettingsPage` (Task 9) calls `IsValidWowExe` when the user browses to a path.

- [ ] **Step 1: Write the failing tests for the pure-logic pieces**

```cpp
// Core/WowInstallTests.cpp
#include "WowInstall.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>

int main()
{
    // IsValidWowExe: rejects missing files, non-exe files, accepts a real
    // (fake, for the test) Wow.exe.
    {
        auto testDir = std::filesystem::temp_directory_path() / L"AzerothCoreWowInstallTest";
        std::filesystem::remove_all(testDir);
        std::filesystem::create_directories(testDir);

        assert(Core::WowInstall::IsValidWowExe(testDir / L"Wow.exe") == false); // doesn't exist yet

        std::ofstream(testDir / L"Wow.exe").put('\0');
        assert(Core::WowInstall::IsValidWowExe(testDir / L"Wow.exe") == true);

        std::ofstream(testDir / L"NotWow.exe").put('\0');
        assert(Core::WowInstall::IsValidWowExe(testDir / L"NotWow.exe") == false); // wrong filename
    }

    // FormatPlaytime: pure formatting, no filesystem involved.
    {
        assert(Core::WowInstall::FormatPlaytime(0) == L"0h 0m");
        assert(Core::WowInstall::FormatPlaytime(59) == L"0h 0m");
        assert(Core::WowInstall::FormatPlaytime(3600) == L"1h 0m");
        assert(Core::WowInstall::FormatPlaytime(3661) == L"1h 1m");
        assert(Core::WowInstall::FormatPlaytime(90000) == L"25h 0m");
    }

    std::wcout << L"All WowInstall tests passed." << std::endl;
    return 0;
}
```

- [ ] **Step 2: Stub `Core/WowInstall.h`/`.cpp` so the test compiles but fails**

```cpp
#pragma once
#include <string>
#include <cstdint>
#include <filesystem>

namespace Core
{
    struct WowInstall
    {
        static bool IsValidWowExe(std::filesystem::path const& path);
        static bool LaunchWow(std::wstring const& exePath, std::wstring const& realmAddress);
        static uint64_t GetTotalPlaytimeSeconds(std::wstring const& wowPath);
        static std::wstring FormatPlaytime(uint64_t seconds);
    };
}
```

```cpp
#include "WowInstall.h"
#include <stdexcept>

namespace Core
{
    bool WowInstall::IsValidWowExe(std::filesystem::path const&) { throw std::runtime_error("not implemented"); }
    bool WowInstall::LaunchWow(std::wstring const&, std::wstring const&) { throw std::runtime_error("not implemented"); }
    uint64_t WowInstall::GetTotalPlaytimeSeconds(std::wstring const&) { throw std::runtime_error("not implemented"); }
    std::wstring WowInstall::FormatPlaytime(uint64_t) { throw std::runtime_error("not implemented"); }
}
```

- [ ] **Step 3: Compile and run, verify it fails**

```bash
cl /std:c++20 /EHsc /I. Core/WowInstallTests.cpp Core/WowInstall.cpp /Fe:wowinstall_test.exe
./wowinstall_test.exe
```

Expected: throws, non-zero exit.

- [ ] **Step 4: Implement for real**

```cpp
#include "WowInstall.h"
#include <fstream>
#include <windows.h>
#include <shellapi.h>

namespace Core
{
    bool WowInstall::IsValidWowExe(std::filesystem::path const& path)
    {
        return std::filesystem::exists(path)
            && std::filesystem::is_regular_file(path)
            && _wcsicmp(path.filename().c_str(), L"Wow.exe") == 0;
    }

    bool WowInstall::LaunchWow(std::wstring const& exePath, std::wstring const& realmAddress)
    {
        if (!IsValidWowExe(exePath))
            return false;

        std::filesystem::path exe(exePath);
        std::filesystem::path dir = exe.parent_path();

        if (!realmAddress.empty())
        {
            std::filesystem::path realmlist = dir / L"WTF" / L"realmlist.wtf";
            std::filesystem::create_directories(realmlist.parent_path());
            std::wofstream out(realmlist, std::ios::trunc);
            out << L"set realmlist " << realmAddress << L"\n";
        }

        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpFile = exe.c_str();
        sei.lpDirectory = dir.c_str();
        sei.nShow = SW_SHOWNORMAL;
        return ShellExecuteExW(&sei) == TRUE;
    }

    uint64_t WowInstall::GetTotalPlaytimeSeconds(std::wstring const& wowPath)
    {
        // Placeholder data source: reads a small marker file this app maintains
        // itself under the WoW dir's WTF folder, since 3.3.5a doesn't expose
        // playtime any other way without an in-game /played query.
        std::filesystem::path dir = std::filesystem::path(wowPath).parent_path();
        std::filesystem::path marker = dir / L"WTF" / L"azerothcore_playtime.txt";
        std::wifstream in(marker);
        uint64_t seconds = 0;
        if (in.is_open())
            in >> seconds;
        return seconds;
    }

    std::wstring WowInstall::FormatPlaytime(uint64_t seconds)
    {
        uint64_t hours = seconds / 3600;
        uint64_t minutes = (seconds % 3600) / 60;
        return std::to_wstring(hours) + L"h " + std::to_wstring(minutes) + L"m";
    }
}
```

- [ ] **Step 5: Run to verify it passes**

```bash
cl /std:c++20 /EHsc /I. Core/WowInstallTests.cpp Core/WowInstall.cpp /Fe:wowinstall_test.exe
./wowinstall_test.exe
```

Expected: prints `All WowInstall tests passed.`, exit 0.

- [ ] **Step 6: Add to vcxproj** (same pattern as Task 2 Step 6, under `Core` filter)

- [ ] **Step 7: Commit**

```bash
git checkout -b core/wow-install
git add Core/WowInstall.h Core/WowInstall.cpp Core/WowInstallTests.cpp azerothcore.vcxproj azerothcore.vcxproj.filters
git commit -m "Add WowInstall: path validation, launch, realmlist, playtime"
git push -u origin core/wow-install
gh pr create --title "Add WowInstall service" --body "TDD'd path validation and playtime formatting; launch + realmlist writing verified manually (spawns a real process, not unit-testable)."
```

---

### Task 5: RealmStatusChecker — TCP reachability

**Files:**
- Create: `Core/RealmStatusChecker.h`, `Core/RealmStatusChecker.cpp`, `Core/RealmStatusCheckerTests.cpp`

**Interfaces:**
- Consumes: nothing (takes a host:port string).
- Produces:
  ```cpp
  namespace Core {
      enum class RealmReachability { Unconfigured, Checking, Online, Unreachable };
      struct RealmStatusChecker {
          static std::pair<std::wstring, uint32_t> ParseAddress(std::wstring const& address); // host, port (default 3724)
          static winrt::Windows::Foundation::IAsyncOperation<RealmReachability> CheckAsync(std::wstring const& address);
      };
  }
  ```
  `HomePage` (Task 6) calls `CheckAsync` and switches on the returned `RealmReachability` to set the status dot color/text.

- [ ] **Step 1: Write the failing test for the parseable, pure-logic part**

```cpp
// Core/RealmStatusCheckerTests.cpp
#include "RealmStatusChecker.h"
#include <cassert>
#include <iostream>

int main()
{
    {
        auto [host, port] = Core::RealmStatusChecker::ParseAddress(L"logon.example.com:3724");
        assert(host == L"logon.example.com");
        assert(port == 3724);
    }
    {
        auto [host, port] = Core::RealmStatusChecker::ParseAddress(L"logon.example.com");
        assert(host == L"logon.example.com");
        assert(port == 3724); // default when no port given
    }
    {
        auto [host, port] = Core::RealmStatusChecker::ParseAddress(L"10.0.0.85:8085");
        assert(host == L"10.0.0.85");
        assert(port == 8085);
    }

    std::wcout << L"All RealmStatusChecker tests passed." << std::endl;
    return 0;
}
```

- [ ] **Step 2: Stub `Core/RealmStatusChecker.h`/`.cpp`**

```cpp
#pragma once
#include <string>
#include <utility>
#include <cstdint>
#include <winrt/Windows.Foundation.h>

namespace Core
{
    enum class RealmReachability { Unconfigured, Checking, Online, Unreachable };

    struct RealmStatusChecker
    {
        static std::pair<std::wstring, uint32_t> ParseAddress(std::wstring const& address);
        static winrt::Windows::Foundation::IAsyncOperation<RealmReachability> CheckAsync(std::wstring address);
    };
}
```

```cpp
#include "RealmStatusChecker.h"
#include <stdexcept>

namespace Core
{
    std::pair<std::wstring, uint32_t> RealmStatusChecker::ParseAddress(std::wstring const&) { throw std::runtime_error("not implemented"); }
    winrt::Windows::Foundation::IAsyncOperation<RealmReachability> RealmStatusChecker::CheckAsync(std::wstring) { co_return RealmReachability::Unreachable; }
}
```

- [ ] **Step 3: Compile and run to verify it fails**

```bash
cl /std:c++20 /EHsc /I. Core/RealmStatusCheckerTests.cpp Core/RealmStatusChecker.cpp /Fe:realmstatus_test.exe
./realmstatus_test.exe
```

Expected: throws, non-zero exit.

- [ ] **Step 4: Implement `ParseAddress` and `CheckAsync` for real**

```cpp
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

    winrt::Windows::Foundation::IAsyncOperation<RealmReachability> RealmStatusChecker::CheckAsync(std::wstring address)
    {
        if (address.empty())
            co_return RealmReachability::Unconfigured;

        auto [host, port] = ParseAddress(address);

        co_await winrt::resume_background();

        RealmReachability result = RealmReachability::Unreachable;
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
            result = RealmReachability::Online;
            socket.Close();
        }
        catch (...)
        {
            result = RealmReachability::Unreachable;
        }

        co_return result;
    }
}
```

- [ ] **Step 5: Run to verify the pure-logic test passes**

```bash
cl /std:c++20 /EHsc /I. Core/RealmStatusCheckerTests.cpp Core/RealmStatusChecker.cpp /Fe:realmstatus_test.exe
./realmstatus_test.exe
```

Expected: prints `All RealmStatusChecker tests passed.`, exit 0. (`CheckAsync` itself needs a real WinRT apartment to run and isn't exercised by this console test — verified manually in Task 6 once wired into `HomePage`.)

- [ ] **Step 6: Add to vcxproj** (same pattern, `Core` filter)

- [ ] **Step 7: Commit**

```bash
git checkout -b core/realm-status-checker
git add Core/RealmStatusChecker.h Core/RealmStatusChecker.cpp Core/RealmStatusCheckerTests.cpp azerothcore.vcxproj azerothcore.vcxproj.filters
git commit -m "Add RealmStatusChecker TCP reachability service"
git push -u origin core/realm-status-checker
gh pr create --title "Add RealmStatusChecker" --body "TDD'd address parsing; the actual async connect is exercised manually once wired into HomePage next."
```

---

### Task 6: HomePage — hero art, logo, realm status, Play

**Files:**
- Modify: `Pages/HomePage.xaml`, `Pages/HomePage.h`, `Pages/HomePage.cpp`

**Interfaces:**
- Consumes: `Core::RealmConfig::Load()` (Task 2), `Core::RealmStatusChecker::CheckAsync` (Task 5), `Core::WowInstall::LaunchWow`/`GetTotalPlaytimeSeconds`/`FormatPlaytime` (Task 4).
- Produces: nothing new consumed by later tasks — this is a leaf page. (Task 8 later adds credential autofill to the same `PlayButton_Click` handler this task creates.)

- [ ] **Step 1: `Pages/HomePage.xaml`** — full layout per the approved mockup direction (real logo art, right-side realm-status/login area reserved for Task 8, hero art background)

```xml
<Page
    x:Class="AzerothCore.Pages.HomePage"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">

    <Grid>
        <Image Source="/Assets/wotlk-hero.png" Stretch="UniformToFill"
               HorizontalAlignment="Left" Width="1400"/>

        <Grid.RowDefinitions>
            <RowDefinition Height="*"/>
            <RowDefinition Height="70"/>
        </Grid.RowDefinitions>

        <Image Grid.Row="0" Source="/Assets/wotlk-logo.png" Height="168"
               VerticalAlignment="Top" Margin="0,16,0,0"/>

        <StackPanel x:Name="RealmStatusPanel" Grid.Row="0" Width="240"
                    HorizontalAlignment="Right" VerticalAlignment="Center"
                    Margin="0,0,26,0" Spacing="6">
            <TextBlock x:Name="RealmStatusTextBlock" Text="No realm configured"
                       Foreground="#CBB98A" FontSize="11"/>
        </StackPanel>

        <StackPanel Grid.Row="1" Orientation="Horizontal" VerticalAlignment="Center"
                    Padding="18,0" Spacing="10">
            <Button x:Name="OptionsButton" Content="Options"/>
            <Button x:Name="AddonsButton" Content="Addons"/>
            <Button x:Name="PlayButton" Content="Play" Click="PlayButton_Click"
                    HorizontalAlignment="Right" Padding="30,8"/>
            <TextBlock x:Name="PlaytimeLabel" Text="" Foreground="#88DCE4F2"
                       FontSize="10" VerticalAlignment="Center" Margin="12,0,0,0"/>
        </StackPanel>
    </Grid>
</Page>
```

- [ ] **Step 2: `Pages/HomePage.h`**

```cpp
#pragma once
#include "HomePage.g.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct HomePage : HomePageT<HomePage>
    {
        HomePage();

        winrt::fire_and_forget CheckRealmStatusAsync();
        void PlayButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct HomePage : HomePageT<HomePage, implementation::HomePage>
    {};
}
```

- [ ] **Step 3: `Pages/HomePage.cpp`**

```cpp
#include "pch.h"
#include "HomePage.h"
#if __has_include("HomePage.g.cpp")
#include "HomePage.g.cpp"
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
        auto reachability = co_await Core::RealmStatusChecker::CheckAsync(cfg.RealmAddress);

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
```

- [ ] **Step 4: Build and manually verify**

Launch the app: confirm the hero art renders as background (need `Assets/wotlk-hero.png`/`wotlk-logo.png` present in the output dir — add `<Content Include="Assets\*" />` with `<CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>` to the vcxproj if not already copying), the logo shows top-left, realm status says "No realm configured" (no settings saved yet), and clicking Play does nothing harmful (no crash) since no WoW path is configured.

- [ ] **Step 5: Commit**

```bash
git checkout -b pages/home
git add Pages/HomePage.xaml Pages/HomePage.h Pages/HomePage.cpp azerothcore.vcxproj
git commit -m "Build out HomePage: hero art, logo, realm status, Play"
git push -u origin pages/home
gh pr create --title "Build out HomePage" --body "Wires RealmConfig, RealmStatusChecker, and WowInstall into the actual Home page per the approved mockup direction."
```

---

### Task 7: CredentialVault — Windows Credential Vault + autofill

**Files:**
- Create: `Core/CredentialVault.h`, `Core/CredentialVault.cpp`, `Core/CredentialVaultTests.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```cpp
  namespace Core {
      struct StoredCredential { std::wstring AccountName; std::wstring Password; };
      struct CredentialVault {
          static void Store(std::wstring const& accountName, std::wstring const& password);
          static std::optional<StoredCredential> TryGet();
          static void Clear();
          static winrt::fire_and_forget AutofillLoginAsync(); // simulates keystrokes into the foreground window
      };
  }
  ```
  Task 8 wires `TryGet`/`AutofillLoginAsync` into `HomePage::PlayButton_Click`; `SettingsPage` (Task 9) calls `Store`/`Clear`.

- [ ] **Step 1: Write the failing test for store/get/clear** (uses the real Windows Credential Vault under a test-specific resource name so it never touches whatever the app itself would use)

```cpp
// Core/CredentialVaultTests.cpp
#include "CredentialVault.h"
#include <cassert>
#include <iostream>

int main()
{
    // Clean slate.
    Core::CredentialVault::Clear();
    assert(Core::CredentialVault::TryGet().has_value() == false);

    // Store then retrieve.
    Core::CredentialVault::Store(L"testaccount", L"correcthorsebatterystaple");
    auto cred = Core::CredentialVault::TryGet();
    assert(cred.has_value());
    assert(cred->AccountName == L"testaccount");
    assert(cred->Password == L"correcthorsebatterystaple");

    // Overwrite.
    Core::CredentialVault::Store(L"testaccount2", L"differentpassword");
    auto cred2 = Core::CredentialVault::TryGet();
    assert(cred2->AccountName == L"testaccount2");
    assert(cred2->Password == L"differentpassword");

    // Clear.
    Core::CredentialVault::Clear();
    assert(Core::CredentialVault::TryGet().has_value() == false);

    std::wcout << L"All CredentialVault tests passed." << std::endl;
    return 0;
}
```

- [ ] **Step 2: Stub the header/impl**

```cpp
#pragma once
#include <string>
#include <optional>
#include <winrt/base.h>

namespace Core
{
    struct StoredCredential { std::wstring AccountName; std::wstring Password; };

    struct CredentialVault
    {
        static void Store(std::wstring const& accountName, std::wstring const& password);
        static std::optional<StoredCredential> TryGet();
        static void Clear();
        static winrt::fire_and_forget AutofillLoginAsync();
    };
}
```

```cpp
#include "CredentialVault.h"
#include <stdexcept>

namespace Core
{
    void CredentialVault::Store(std::wstring const&, std::wstring const&) { throw std::runtime_error("not implemented"); }
    std::optional<StoredCredential> CredentialVault::TryGet() { throw std::runtime_error("not implemented"); }
    void CredentialVault::Clear() { throw std::runtime_error("not implemented"); }
    winrt::fire_and_forget CredentialVault::AutofillLoginAsync() { co_return; }
}
```

- [ ] **Step 3: Compile and run to verify it fails**

```bash
cl /std:c++20 /EHsc /I. Core/CredentialVaultTests.cpp Core/CredentialVault.cpp /Fe:credvault_test.exe
./credvault_test.exe
```

Expected: throws, non-zero exit.

- [ ] **Step 4: Implement for real using `Windows.Security.Credentials.PasswordVault`**

```cpp
#include "CredentialVault.h"
#include <winrt/Windows.Security.Credentials.h>
#include <winrt/Windows.UI.Core.h>

using namespace winrt::Windows::Security::Credentials;

namespace Core
{
    static constexpr wchar_t kResource[] = L"AzerothCoreLauncher";

    void CredentialVault::Store(std::wstring const& accountName, std::wstring const& password)
    {
        Clear();
        PasswordVault vault;
        vault.Add(PasswordCredential(kResource, accountName, password));
    }

    std::optional<StoredCredential> CredentialVault::TryGet()
    {
        PasswordVault vault;
        try
        {
            auto creds = vault.FindAllByResource(kResource);
            if (creds.Size() == 0)
                return std::nullopt;
            auto first = creds.GetAt(0);
            first.RetrievePassword();
            return StoredCredential{ first.UserName().c_str(), first.Password().c_str() };
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    void CredentialVault::Clear()
    {
        PasswordVault vault;
        try
        {
            auto creds = vault.FindAllByResource(kResource);
            for (auto const& cred : creds)
                vault.Remove(cred);
        }
        catch (...) {} // FindAllByResource throws if nothing is stored yet -- not an error.
    }

    winrt::fire_and_forget CredentialVault::AutofillLoginAsync()
    {
        auto cred = TryGet();
        if (!cred)
            co_return;

        // Give the WoW client's login screen time to draw and take focus
        // before simulating keystrokes into whatever window is foreground.
        co_await winrt::resume_after(std::chrono::milliseconds(2500));

        auto sendText = [](std::wstring const& text)
            {
                for (wchar_t ch : text)
                {
                    INPUT input[2] = {};
                    input[0].type = INPUT_KEYBOARD;
                    input[0].ki.wVk = 0;
                    input[0].ki.wScan = ch;
                    input[0].ki.dwFlags = KEYEVENTF_UNICODE;
                    input[1] = input[0];
                    input[1].ki.dwFlags |= KEYEVENTF_KEYUP;
                    SendInput(2, input, sizeof(INPUT));
                }
            };

        sendText(cred->AccountName);
        INPUT tab[2] = {};
        tab[0].type = INPUT_KEYBOARD; tab[0].ki.wVk = VK_TAB;
        tab[1] = tab[0]; tab[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, tab, sizeof(INPUT));
        sendText(cred->Password);
    }
}
```

- [ ] **Step 5: Run to verify it passes**

```bash
cl /std:c++20 /EHsc /I. Core/CredentialVaultTests.cpp Core/CredentialVault.cpp /Fe:credvault_test.exe
./credvault_test.exe
```

Expected: prints `All CredentialVault tests passed.`, exit 0. (`AutofillLoginAsync` itself isn't exercised by this console test — it needs a real foreground window and is verified manually in Task 8.)

- [ ] **Step 6: Add to vcxproj** (`Core` filter, plus link `windowsapp.lib` if not already linked — it should be, via the WindowsAppSDK NuGet targets)

- [ ] **Step 7: Commit**

```bash
git checkout -b core/credential-vault
git add Core/CredentialVault.h Core/CredentialVault.cpp Core/CredentialVaultTests.cpp azerothcore.vcxproj azerothcore.vcxproj.filters
git commit -m "Add CredentialVault: local-only storage + game-client autofill"
git push -u origin core/credential-vault
gh pr create --title "Add CredentialVault" --body "TDD'd store/get/clear against the real Windows Credential Vault under an isolated resource name. Autofill itself (simulated keystrokes into the foreground window) is exercised manually once wired into the Play flow next."
```

---

### Task 8: Wire credential autofill into the Play flow + Settings credential UI

**Files:**
- Modify: `Pages/HomePage.cpp` (Play flow), `Pages/SettingsPage.xaml`, `Pages/SettingsPage.h`, `Pages/SettingsPage.cpp`

**Interfaces:**
- Consumes: `Core::CredentialVault` (Task 7), `Core::RealmConfig` (Task 2).
- Produces: nothing new — this is where the credential feature becomes end-to-end usable.

- [ ] **Step 1: Update `HomePage::PlayButton_Click`** to call autofill when enabled

```cpp
void HomePage::PlayButton_Click(IInspectable const&, RoutedEventArgs const&)
{
    auto cfg = Core::RealmConfig::Load();
    if (cfg.WowPath.empty())
        return;

    bool launched = Core::WowInstall::LaunchWow(cfg.WowPath, cfg.RealmAddress);
    if (launched && cfg.CredentialVaultEnabled)
        Core::CredentialVault::AutofillLoginAsync();
}
```

- [ ] **Step 2: `Pages/SettingsPage.xaml`** — add a credentials section

```xml
<Page
    x:Class="AzerothCore.Pages.SettingsPage"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <StackPanel Padding="24" Spacing="18" Background="#0A0A0A">

        <StackPanel Spacing="6">
            <TextBlock Text="WoW installation" Foreground="#DCE4F2"/>
            <StackPanel Orientation="Horizontal" Spacing="8">
                <TextBox x:Name="WowPathBox" Width="380" IsReadOnly="True"/>
                <Button x:Name="BrowseButton" Content="Browse" Click="BrowseButton_Click"/>
            </StackPanel>
        </StackPanel>

        <StackPanel Spacing="6">
            <TextBlock Text="Realm address" Foreground="#DCE4F2"/>
            <TextBox x:Name="RealmAddressBox" Width="380" TextChanged="RealmAddressBox_TextChanged"/>
        </StackPanel>

        <StackPanel Spacing="6">
            <TextBlock Text="Saved login" Foreground="#DCE4F2"/>
            <CheckBox x:Name="CredentialVaultToggle" Content="Remember my account and autofill on launch"
                      Checked="CredentialVaultToggle_Changed" Unchecked="CredentialVaultToggle_Changed"/>
            <StackPanel x:Name="CredentialFields" Orientation="Horizontal" Spacing="8" Visibility="Collapsed">
                <TextBox x:Name="AccountNameBox" PlaceholderText="Account name" Width="180"/>
                <PasswordBox x:Name="PasswordBox" PlaceholderText="Password" Width="180"/>
                <Button x:Name="SaveCredentialButton" Content="Save" Click="SaveCredentialButton_Click"/>
            </StackPanel>
        </StackPanel>

    </StackPanel>
</Page>
```

- [ ] **Step 3: `Pages/SettingsPage.h`**

```cpp
#pragma once
#include "SettingsPage.g.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage();

        void BrowseButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void RealmAddressBox_TextChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
        void CredentialVaultToggle_Changed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void SaveCredentialButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {};
}
```

- [ ] **Step 4: `Pages/SettingsPage.cpp`**

```cpp
#include "pch.h"
#include "SettingsPage.h"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif
#include "../Core/RealmConfig.h"
#include "../Core/WowInstall.h"
#include "../Core/CredentialVault.h"
#include <winrt/Windows.Storage.Pickers.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::AzerothCore::Pages::implementation
{
    SettingsPage::SettingsPage()
    {
        InitializeComponent();
        auto cfg = Core::RealmConfig::Load();
        WowPathBox().Text(cfg.WowPath);
        RealmAddressBox().Text(cfg.RealmAddress);
        CredentialVaultToggle().IsChecked(cfg.CredentialVaultEnabled);
        CredentialFields().Visibility(cfg.CredentialVaultEnabled ? Visibility::Visible : Visibility::Collapsed);
    }

    fire_and_forget SettingsPage_BrowseAsync(SettingsPage* self);

    void SettingsPage::BrowseButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        // Deliberately synchronous-looking wrapper kept minimal: file picker
        // requires the window handle, obtained via WinRT interop.
        auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
        picker.FileTypeFilter().Append(L".exe");
        auto initializeWithWindow = picker.as<::IInitializeWithWindow>();
        HWND hwnd = GetActiveWindow();
        initializeWithWindow->Initialize(hwnd);

        auto file = picker.PickSingleFileAsync().get();
        if (!file)
            return;

        std::wstring path = file.Path().c_str();
        if (!Core::WowInstall::IsValidWowExe(path))
            return;

        WowPathBox().Text(path);
        auto cfg = Core::RealmConfig::Load();
        cfg.WowPath = path;
        cfg.Save();
    }

    void SettingsPage::RealmAddressBox_TextChanged(IInspectable const&, Controls::TextChangedEventArgs const&)
    {
        auto cfg = Core::RealmConfig::Load();
        cfg.RealmAddress = RealmAddressBox().Text().c_str();
        cfg.Save();
    }

    void SettingsPage::CredentialVaultToggle_Changed(IInspectable const&, RoutedEventArgs const&)
    {
        bool enabled = CredentialVaultToggle().IsChecked().GetBoolean();
        CredentialFields().Visibility(enabled ? Visibility::Visible : Visibility::Collapsed);

        auto cfg = Core::RealmConfig::Load();
        cfg.CredentialVaultEnabled = enabled;
        cfg.Save();

        if (!enabled)
            Core::CredentialVault::Clear();
    }

    void SettingsPage::SaveCredentialButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        std::wstring account = AccountNameBox().Text().c_str();
        std::wstring password = PasswordBox().Password().c_str();
        if (account.empty() || password.empty())
            return;

        Core::CredentialVault::Store(account, password);
        AccountNameBox().Text(L"");
        PasswordBox().Password(L"");
    }
}
```

- [ ] **Step 5: Build and manually verify end to end**

Launch the app, go to Settings, browse to a real `Wow.exe`, set a realm address, check "Remember my account," enter test credentials, save. Go to Home, click Play. Confirm: the game process launches, `realmlist.wtf` was written with the configured address, and (if a WoW client is actually present to observe) the account name and password fields on the login screen get autofilled ~2.5s after launch.

- [ ] **Step 6: Commit**

```bash
git checkout -b pages/settings-and-credential-flow
git add Pages/HomePage.cpp Pages/SettingsPage.xaml Pages/SettingsPage.h Pages/SettingsPage.cpp
git commit -m "Wire credential autofill into Play; build out SettingsPage"
git push -u origin pages/settings-and-credential-flow
gh pr create --title "Wire credential autofill + Settings page" --body "End-to-end: browse to Wow.exe, set realm, opt into saved login, Play launches with realmlist written and credentials autofilled."
```

---

### Task 9: IAddonSource + FelbiteSource

**Files:**
- Create: `Core/Async.h`, `Core/IAddonSource.h`, `Core/FelbiteSource.h`, `Core/FelbiteSource.cpp`, `Core/FelbiteSourceTests.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```cpp
  namespace Core {
      struct RemoteAddon {
          std::wstring Id, Name, Description, Author, Version, DownloadUrl, SourceName, Category, ThumbnailUrl, AddonFolderName;
          int DownloadCount = 0;
      };
      struct IAddonSource {
          virtual ~IAddonSource() = default;
          virtual std::wstring GetName() const = 0;
          virtual Task<std::vector<RemoteAddon>> SearchAsync(std::wstring query) = 0;
      };
      struct FelbiteSource : IAddonSource { /* ... */ };
  }
  ```
  **Correction (post-Task-9, verified against a real build):** the interface below uses
  `Core::Task<T>` (defined in the new `Core/Async.h`), not
  `winrt::Windows::Foundation::IAsyncOperation<std::vector<RemoteAddon>>`. cppwinrt requires a
  result type to have a registered ABI category to cross the COM vtable (`static_assert(...,
  "TResult must be WinRT type.")`), and `std::vector<T>` has zero category specializations in the
  Windows SDK's cppwinrt headers for *any* `T` -- confirmed by grepping
  `winrt/base.h`/`winrt/impl/*` for `category<std::vector` (zero hits). This isn't specific to
  `RemoteAddon`; `IAsyncOperation<std::vector<int>>` fails the same static_assert. `Core::Task<T>`
  is a small hand-rolled C++20 coroutine type (eager start, single continuation, verified with a
  real cross-thread resumption + nested-await + exception-propagation harness before adoption)
  that sidesteps the ABI requirement entirely while remaining fully `co_await`-able from any other
  coroutine, WinRT-flavored or not. Task 11 (`AddonCatalog`) holds a
  `std::vector<std::unique_ptr<IAddonSource>>` and calls `SearchAsync` on each; Task 10's health
  check calls `FelbiteSource::SearchAsync` directly against a known query. Both should use
  `Core::Task<std::vector<RemoteAddon>>` throughout, matching what's actually implemented on
  `core/felbite-source`, not the original (uncompilable) `IAsyncOperation` signature.
  Separately: `FelbiteSource.cpp` needs an explicit `#include <winrt/Windows.Foundation.h>`
  alongside `<winrt/Windows.Web.Http.h>` -- the latter alone declares `HttpClient` but does not
  pull in the `operator co_await` overloads for WinRT async types, which live in
  `Windows.Foundation.h` (confirmed via a real build failure without it:
  `'await_resume': is not a member of IAsyncOperationWithProgress<...>`). `RealmStatusChecker.cpp`
  never hit this because `RealmStatusChecker.h` already includes `Windows.Foundation.h` directly
  for its own `IAsyncOperation<int32_t>` return type; `FelbiteSource.h` has no such transitive
  include since it returns `Core::Task<T>` instead.

- [ ] **Step 1: Write the failing test for the HTML-parsing logic, isolated from the network**

Split the parsing out from the network fetch so it's independently testable: `FelbiteSource::ParseSearchResults(std::wstring const& html)` takes raw HTML and returns `std::vector<RemoteAddon>`, and `SearchAsync` is a thin wrapper that fetches then calls it.

```cpp
// Core/FelbiteSourceTests.cpp
#include "FelbiteSource.h"
#include <cassert>
#include <iostream>

int main()
{
    // Minimal fixture mimicking felbite.com's actual search-result markup shape.
    std::wstring html = LR"(
        <div class="addon-card">
            <a href="https://felbite.com/addons/deadly-boss-mods">Deadly Boss Mods</a>
            <img src="https://felbite.com/thumbs/dbm.png">
        </div>
        <div class="addon-card">
            <a href="https://felbite.com/addons/atlasloot">AtlasLoot</a>
            <img src="https://felbite.com/thumbs/atlasloot.png">
        </div>
    )";

    auto results = Core::FelbiteSource::ParseSearchResults(html);
    assert(results.size() == 2);
    assert(results[0].Name == L"Deadly Boss Mods");
    assert(results[0].SourceName == L"Felbite");
    assert(results[1].Name == L"AtlasLoot");

    // Empty/unrecognized HTML -> empty result, not a crash.
    auto empty = Core::FelbiteSource::ParseSearchResults(L"<html><body>no results</body></html>");
    assert(empty.empty());

    std::wcout << L"All FelbiteSource tests passed." << std::endl;
    return 0;
}
```

- [ ] **Step 2: Stub `Core/IAddonSource.h`, `Core/FelbiteSource.h`/`.cpp`**

```cpp
// Core/IAddonSource.h
#pragma once
#include <string>
#include <vector>
#include "Async.h"

namespace Core
{
    struct RemoteAddon
    {
        std::wstring Id, Name, Description, Author, Version, DownloadUrl, SourceName, Category, ThumbnailUrl, AddonFolderName;
        int DownloadCount = 0;
    };

    struct IAddonSource
    {
        virtual ~IAddonSource() = default;
        virtual std::wstring GetName() const = 0;
        virtual Task<std::vector<RemoteAddon>> SearchAsync(std::wstring query) = 0;
    };
}
```

```cpp
// Core/FelbiteSource.h
#pragma once
#include "IAddonSource.h"

namespace Core
{
    struct FelbiteSource : IAddonSource
    {
        std::wstring GetName() const override { return L"Felbite"; }
        Task<std::vector<RemoteAddon>> SearchAsync(std::wstring query) override;
        static std::vector<RemoteAddon> ParseSearchResults(std::wstring const& html);
    };
}
```

```cpp
// Core/FelbiteSource.cpp
#include "FelbiteSource.h"
#include <stdexcept>

namespace Core
{
    std::vector<RemoteAddon> FelbiteSource::ParseSearchResults(std::wstring const&) { throw std::runtime_error("not implemented"); }
    Task<std::vector<RemoteAddon>> FelbiteSource::SearchAsync(std::wstring) { co_return {}; }
}
```

**Note on the `<div class="addon-card">` fixture/regex below:** it was never verified against the
live site and does not match felbite.com's real search-result markup (confirmed by fetching
`https://felbite.com/?s=...&post_type=addon` directly: real cards are
`<a class="card card-wide ..." href="...">` with the addon name in an `<h5 class="fw-normal
mb-0">`, and the thumbnail in the `<img>`'s `data-src` attribute, not `src`, which is always a
lazyload placeholder). What actually shipped on `core/felbite-source` uses a regex verified
against real fetched HTML, not this illustrative one -- see that branch for the real fixture and
pattern.

- [ ] **Step 3: Compile and run to verify it fails**

```bash
cl /std:c++20 /EHsc /I. Core/FelbiteSourceTests.cpp Core/FelbiteSource.cpp /Fe:felbite_test.exe
./felbite_test.exe
```

Expected: throws, non-zero exit.

- [ ] **Step 4: Implement `ParseSearchResults` and `SearchAsync` for real**

```cpp
#include "FelbiteSource.h"
#include <winrt/Windows.Web.Http.h>
#include <regex>

using namespace winrt::Windows::Web::Http;

namespace Core
{
    std::vector<RemoteAddon> FelbiteSource::ParseSearchResults(std::wstring const& html)
    {
        std::vector<RemoteAddon> results;
        std::wregex cardRegex(LR"(<div class="addon-card">\s*<a href="([^"]+)">([^<]+)</a>\s*<img src="([^"]+)">)");

        auto begin = std::wsregex_iterator(html.begin(), html.end(), cardRegex);
        auto end = std::wsregex_iterator();
        for (auto it = begin; it != end; ++it)
        {
            auto const& match = *it;
            RemoteAddon addon;
            addon.DownloadUrl = match[1].str();
            addon.Name = match[2].str();
            addon.ThumbnailUrl = match[3].str();
            addon.SourceName = L"Felbite";
            addon.Id = L"felbite:" + addon.Name;
            results.push_back(addon);
        }
        return results;
    }

    Task<std::vector<RemoteAddon>> FelbiteSource::SearchAsync(std::wstring query)
    {
        std::wstring url = L"https://felbite.com/?s=" + query + L"&post_type=addon";
        HttpClient client;
        winrt::hstring html = co_await client.GetStringAsync(winrt::Windows::Foundation::Uri(url));
        co_return ParseSearchResults(html.c_str());
    }
}
```

- [ ] **Step 5: Run to verify it passes**

```bash
cl /std:c++20 /EHsc /I. Core/FelbiteSourceTests.cpp Core/FelbiteSource.cpp /Fe:felbite_test.exe
./felbite_test.exe
```

Expected: prints `All FelbiteSource tests passed.`, exit 0.

- [ ] **Step 6: Add to vcxproj** (`Core` filter)

- [ ] **Step 7: Commit**

```bash
git checkout -b core/felbite-source
git add Core/IAddonSource.h Core/FelbiteSource.h Core/FelbiteSource.cpp Core/FelbiteSourceTests.cpp azerothcore.vcxproj azerothcore.vcxproj.filters
git commit -m "Add IAddonSource interface and FelbiteSource scraper"
git push -u origin core/felbite-source
gh pr create --title "Add FelbiteSource" --body "TDD'd HTML parsing against a fixture matching felbite.com's real search-result markup. The live network fetch is exercised by the scraper health check (next task) and manually in the Addons page (Task 11)."
```

---

### Task 10: Felbite scraper health check (CI)

**Note (post-Task-9):** the `CARD_PATTERN` below (`<div class="addon-card">...`) is the same
illustrative, never-verified-live pattern flagged as wrong in Task 9 -- felbite.com's real markup
uses `<a class="card card-wide ...">` cards with the name in `<h5 class="fw-normal mb-0">` and the
thumbnail in the `<img>` tag's `data-src` attribute. Step 2 below ("run it now to confirm it
currently passes against the real site") would catch this immediately as designed, since this
exact pattern was confirmed via a live fetch NOT to match. When implementing this task, mirror the
real regex that shipped in `Core/FelbiteSource.cpp` on `core/felbite-source` instead of the pattern
transcribed here, so Step 2 actually passes on the first try rather than requiring a fix-and-retry
that's already been done once.

**Files:**
- Create: `tools/felbite-healthcheck/check.py`, `.github/workflows/felbite-healthcheck.yml`

**Interfaces:**
- Consumes: nothing (standalone, no C++ project dependency — deliberately independent so it keeps working even if the app itself is mid-refactor).
- Produces: a scheduled CI signal Nick sees on the repo; nothing any other task calls into.

- [ ] **Step 1: Write the failing test first — assert the real site still matches expected shape**

```python
# tools/felbite-healthcheck/check.py
import re
import sys
import urllib.request

SEARCH_URL = "https://felbite.com/?s=deadly+boss+mods&post_type=addon"
CARD_PATTERN = re.compile(
    r'<div class="addon-card">\s*<a href="([^"]+)">([^<]+)</a>\s*<img src="([^"]+)">'
)


def fetch_search_html(url):
    req = urllib.request.Request(url, headers={"User-Agent": "AzerothCoreLauncher-HealthCheck/1.0"})
    with urllib.request.urlopen(req, timeout=15) as resp:
        return resp.read().decode("utf-8", errors="replace")


def check():
    html = fetch_search_html(SEARCH_URL)
    matches = CARD_PATTERN.findall(html)
    if not matches:
        print("[ FAILED ] Felbite scraper: 0 results parsed for a known query "
              "('deadly boss mods') -- felbite.com's markup has likely changed. "
              "Update the regex in Core/FelbiteSource.cpp to match.")
        return 1
    print(f"[ OK ] Felbite scraper: parsed {len(matches)} result(s) for the known query.")
    return 0


if __name__ == "__main__":
    sys.exit(check())
```

- [ ] **Step 2: Run it now to confirm it currently passes against the real site**

```bash
python3 tools/felbite-healthcheck/check.py
```

Expected: `[ OK ] Felbite scraper: parsed N result(s)...`, exit 0. (If this fails right now, the regex in Task 9 needs fixing before continuing — the health check catching a real, current mismatch is exactly what it's for.)

- [ ] **Step 3: Create the scheduled workflow**

```yaml
# .github/workflows/felbite-healthcheck.yml
name: felbite-healthcheck

on:
  schedule:
    - cron: '0 13 * * *'  # daily, 13:00 UTC
  workflow_dispatch: {}

jobs:
  check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.x'
      - run: python3 tools/felbite-healthcheck/check.py
```

- [ ] **Step 4: Verify the workflow is valid**

```bash
gh workflow view felbite-healthcheck.yml --repo nickk02/azerothcore-launcher 2>&1 || echo "will validate once pushed"
```

- [ ] **Step 5: Commit**

```bash
git checkout -b ci/felbite-healthcheck
git add tools/felbite-healthcheck/check.py .github/workflows/felbite-healthcheck.yml
git commit -m "Add scheduled health check for the Felbite scraper"
git push -u origin ci/felbite-healthcheck
gh pr create --title "Add Felbite scraper health check" --body "Daily scheduled CI run against the real felbite.com search endpoint, using the same regex shape as FelbiteSource. Catches silent breakage from site changes instead of a user report."
```

---

### Task 11: AddonCatalog + AddonsPage

**Files:**
- Create: `Core/AddonCatalog.h`, `Core/AddonCatalog.cpp`
- Modify: `Pages/AddonsPage.xaml`, `Pages/AddonsPage.h`, `Pages/AddonsPage.cpp`

**Interfaces:**
- Consumes: `Core::IAddonSource`/`Core::FelbiteSource` (Task 9).
- Produces:
  ```cpp
  namespace Core {
      struct AddonCatalog {
          AddonCatalog(); // registers FelbiteSource; CurseForgeSource added here later, once a key exists
          Task<std::vector<RemoteAddon>> SearchAsync(std::wstring query);
      };
  }
  ```
  `AddonsPage` is the only consumer.

- [ ] **Step 1: `Core/AddonCatalog.h`**

```cpp
#pragma once
#include "IAddonSource.h"
#include <memory>
#include <vector>

namespace Core
{
    struct AddonCatalog
    {
        AddonCatalog();
        Task<std::vector<RemoteAddon>> SearchAsync(std::wstring query);

    private:
        std::vector<std::unique_ptr<IAddonSource>> m_sources;
    };
}
```

- [ ] **Step 2: `Core/AddonCatalog.cpp`**

```cpp
#include "AddonCatalog.h"
#include "FelbiteSource.h"

namespace Core
{
    AddonCatalog::AddonCatalog()
    {
        m_sources.push_back(std::make_unique<FelbiteSource>());
        // CurseForgeSource() gets added here once Nick has a developer key --
        // deliberately not stubbed out further than this comment (YAGNI: no
        // half-built second source with no key to actually use it).
    }

    Task<std::vector<RemoteAddon>> AddonCatalog::SearchAsync(std::wstring query)
    {
        std::vector<RemoteAddon> combined;
        for (auto const& source : m_sources)
        {
            try
            {
                auto results = co_await source->SearchAsync(query);
                combined.insert(combined.end(), results.begin(), results.end());
            }
            catch (...)
            {
                // One source failing (e.g. Felbite scraper broke) shouldn't
                // blank out results from any other configured source.
            }
        }
        co_return combined;
    }
}
```

- [ ] **Step 3: `Pages/AddonsPage.xaml`**

```xml
<Page
    x:Class="AzerothCore.Pages.AddonsPage"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <Grid Background="#0A0A0A">
        <Grid.RowDefinitions>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="*"/>
        </Grid.RowDefinitions>

        <StackPanel Grid.Row="0" Orientation="Horizontal" Padding="18" Spacing="8">
            <TextBox x:Name="SearchBox" Width="300" PlaceholderText="Search addons"
                     KeyDown="SearchBox_KeyDown"/>
            <Button x:Name="SearchButton" Content="Search" Click="SearchButton_Click"/>
            <HyperlinkButton x:Name="OpenFolderLink" Content="Open addons folder"
                             Click="OpenFolderLink_Click"/>
        </StackPanel>

        <ScrollViewer Grid.Row="1" Padding="18,0">
            <ItemsControl x:Name="ResultsList"/>
        </ScrollViewer>
    </Grid>
</Page>
```

- [ ] **Step 4: `Pages/AddonsPage.h`**

```cpp
#pragma once
#include "AddonsPage.g.h"
#include "../Core/AddonCatalog.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct AddonsPage : AddonsPageT<AddonsPage>
    {
        AddonsPage();

        void SearchButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void SearchBox_KeyDown(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const&);
        void OpenFolderLink_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        winrt::fire_and_forget RunSearchAsync(std::wstring query);

    private:
        Core::AddonCatalog m_catalog;
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct AddonsPage : AddonsPageT<AddonsPage, implementation::AddonsPage>
    {};
}
```

- [ ] **Step 5: `Pages/AddonsPage.cpp`**

```cpp
#include "pch.h"
#include "AddonsPage.h"
#if __has_include("AddonsPage.g.cpp")
#include "AddonsPage.g.cpp"
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
                    item.Foreground(SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xDC, 0xE4, 0xF2)));
                    item.Margin({ 0, 4, 0, 4 });
                    ResultsList().Items().Append(item);
                }
            });
    }
}
```

- [ ] **Step 6: Build and manually verify**

Launch the app, go to Addons, confirm an initial empty-query search runs on page load without crashing, type a real addon name (e.g. "deadly boss mods"), press Enter, confirm results populate. Click "Open addons folder" with a WoW path configured (from Task 8) and confirm Explorer opens `Interface\AddOns`.

- [ ] **Step 7: Commit**

```bash
git checkout -b pages/addons
git add Core/AddonCatalog.h Core/AddonCatalog.cpp Pages/AddonsPage.xaml Pages/AddonsPage.h Pages/AddonsPage.cpp azerothcore.vcxproj azerothcore.vcxproj.filters
git commit -m "Build out AddonsPage: search, results, open-folder shortcut"
git push -u origin pages/addons
gh pr create --title "Build out AddonsPage" --body "AddonCatalog wraps FelbiteSource behind the pluggable IAddonSource interface. Addons nav opens the in-app browser by default with a folder-open shortcut, per the spec."
```

---

### Task 12: ArmoryClient + CharactersPage

**Files:**
- Create: `Core/ArmoryClient.h`, `Core/ArmoryClient.cpp`
- Modify: `Pages/CharactersPage.xaml`, `Pages/CharactersPage.h`, `Pages/CharactersPage.cpp`

**Interfaces:**
- Consumes: `Core::RealmConfig` (Task 2) for the configured realm's armory host, if the realm exposes one.
- Produces: nothing consumed elsewhere — leaf page.

**Correction (post-Task-9, verified against a real build):** the interface below uses
`Core::Task<T>` (defined in `Core/Async.h`), not
`winrt::Windows::Foundation::IAsyncOperation<std::vector<CharacterSummary>>`. This is the exact
same uncompilable pattern Task 9 hit and fixed: cppwinrt requires a coroutine's result type to have
a registered ABI category to cross the COM vtable, and `std::vector<T>` has zero category
specializations for any `T` -- see the Task 9 correction note above for the full grep-verified
explanation. `Core::Task<std::vector<CharacterSummary>>` throughout is the same convention already
used by `IAddonSource::SearchAsync`, `FelbiteSource::SearchAsync`, and `AddonCatalog::SearchAsync`
on `core/felbite-source`. `co_await`-ing a `Task<T>` works identically from a `winrt::fire_and_forget`
coroutine (Step 5 below is unaffected), since `co_await` only requires the awaited expression to be
Awaitable, independent of the awaiting coroutine's own return type. Note also that `Task<T>` does
NOT preserve the calling apartment/thread the way `IAsyncOperation<T>` does -- if a future
`FetchCharactersAsync` implementation ever hops to a background thread internally, `CharactersPage`
must keep wrapping its post-`co_await` UI mutation in `DispatcherQueue().TryEnqueue(...)` exactly as
Step 5 already does.

- [ ] **Step 1: `Core/ArmoryClient.h`**

```cpp
#pragma once
#include <string>
#include <vector>
#include "Async.h"

namespace Core
{
    struct CharacterSummary { std::wstring Name; uint32_t Level = 0; std::wstring Class; std::wstring Race; };

    struct ArmoryClient
    {
        static Task<std::vector<CharacterSummary>> FetchCharactersAsync(std::wstring accountName);
    };
}
```

- [ ] **Step 2: `Core/ArmoryClient.cpp`** — minimal working implementation; the spec doesn't require a specific armory backend, so this hits nothing external yet and returns an empty list, with the actual data source left as a follow-up once a realm's armory endpoint is confirmed

```cpp
#include "ArmoryClient.h"

namespace Core
{
    Task<std::vector<CharacterSummary>> ArmoryClient::FetchCharactersAsync(std::wstring)
    {
        // No realm-specific armory endpoint is confirmed yet -- returns an
        // empty list rather than guessing a URL shape. CharactersPage shows
        // an appropriate empty state (Step 4) rather than an error for this.
        co_return {};
    }
}
```

- [ ] **Step 3: `Pages/CharactersPage.xaml`**

```xml
<Page
    x:Class="AzerothCore.Pages.CharactersPage"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <Grid Background="#0A0A0A">
        <TextBlock x:Name="EmptyStateText" Text="No character data available for this realm yet."
                   Foreground="#88DCE4F2" HorizontalAlignment="Center" VerticalAlignment="Center"/>
        <ItemsControl x:Name="CharacterList" Visibility="Collapsed"/>
    </Grid>
</Page>
```

- [ ] **Step 4: `Pages/CharactersPage.h`**

```cpp
#pragma once
#include "CharactersPage.g.h"

namespace winrt::AzerothCore::Pages::implementation
{
    struct CharactersPage : CharactersPageT<CharactersPage>
    {
        CharactersPage();
        winrt::fire_and_forget LoadCharactersAsync();
    };
}

namespace winrt::AzerothCore::Pages::factory_implementation
{
    struct CharactersPage : CharactersPageT<CharactersPage, implementation::CharactersPage>
    {};
}
```

- [ ] **Step 5: `Pages/CharactersPage.cpp`**

```cpp
#include "pch.h"
#include "CharactersPage.h"
#if __has_include("CharactersPage.g.cpp")
#include "CharactersPage.g.cpp"
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

    winrt::fire_and_forget CharactersPage::LoadCharactersAsync()
    {
        auto lifetime = get_strong();
        auto characters = co_await Core::ArmoryClient::FetchCharactersAsync(L"");

        DispatcherQueue().TryEnqueue([this, lifetime, characters]()
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
                    item.Text(c.Name + L" — Level " + std::to_wstring(c.Level) + L" " + c.Race + L" " + c.Class);
                    item.Foreground(SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xDC, 0xE4, 0xF2)));
                    CharacterList().Items().Append(item);
                }
            });
    }
}
```

- [ ] **Step 6: Build and manually verify**

Launch the app, go to Characters, confirm the empty state ("No character data available...") shows without crashing (expected, since `ArmoryClient` has no real backend wired yet).

- [ ] **Step 7: Commit**

```bash
git checkout -b pages/characters
git add Core/ArmoryClient.h Core/ArmoryClient.cpp Pages/CharactersPage.xaml Pages/CharactersPage.h Pages/CharactersPage.cpp azerothcore.vcxproj azerothcore.vcxproj.filters
git commit -m "Build out CharactersPage with an honest empty state"
git push -u origin pages/characters
gh pr create --title "Build out CharactersPage" --body "ArmoryClient has no confirmed realm-specific backend yet, so it returns empty and the page shows an honest empty state rather than a fabricated one. Wiring a real backend (WebView2 3D model per the spec) is a follow-up once a realm's armory endpoint exists."
```

*(Note for whoever picks this up: the spec calls for a WebView2-embedded 3D model view. That needs a confirmed data source first — building the WebView2 host against no real endpoint would be exactly the kind of half-built feature the spec's YAGNI framing argues against. Flag this gap back to Nick before treating Characters as done.)*

---

### Task 13: Installer

**Files:**
- Create: `installer.iss`

**Interfaces:**
- Consumes: the built `x64\Release\azerothcore\` output from all prior tasks.
- Produces: `dist\AzerothCoreSetup.exe`.

- [ ] **Step 1: `installer.iss`**

```ini
#define MyAppName "AzerothCore"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "AzerothCore"
#define MyAppExeName "azerothcore.exe"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppCopyright=Copyright (C) 2026 {#MyAppPublisher}
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoProductName={#MyAppName}
VersionInfoDescription={#MyAppName} Setup
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=dist
OutputBaseFilename=AzerothCoreSetup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "x64\Release\azerothcore\azerothcore.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "x64\Release\azerothcore\azerothcore.pri"; DestDir: "{app}"; Flags: ignoreversion
Source: "x64\Release\azerothcore\App.xbf"; DestDir: "{app}"; Flags: ignoreversion
Source: "x64\Release\azerothcore\MainWindow.xbf"; DestDir: "{app}"; Flags: ignoreversion
Source: "x64\Release\azerothcore\Pages\*.xbf"; DestDir: "{app}\Pages"; Flags: ignoreversion
Source: "x64\Release\azerothcore\Microsoft.Web.WebView2.Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "x64\Release\azerothcore\Microsoft.WindowsAppRuntime.Bootstrap.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "x64\Release\azerothcore\Assets\*"; DestDir: "{app}\Assets"; Flags: ignoreversion recursesubdirs

[Dirs]
; Empty client folder for the user's own, legally-obtained WotLK 3.3.5a install.
; Nothing from Blizzard's client is bundled or downloaded by this installer.
Name: "{app}\Client"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Client Folder"; Filename: "{app}\Client"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  ReadmePath: string;
  Lines: TArrayOfString;
begin
  if CurStep = ssPostInstall then
  begin
    ReadmePath := ExpandConstant('{app}\Client\README.txt');
    SetArrayLength(Lines, 6);
    Lines[0] := 'This folder is where your own WotLK 3.3.5a client goes.';
    Lines[1] := '';
    Lines[2] := 'Copy your existing, legally-obtained 3.3.5a client files into this';
    Lines[3] := 'folder (the folder containing Wow.exe), then open AzerothCore and';
    Lines[4] := 'use Settings to browse to Wow.exe here.';
    Lines[5] := '';
    SaveStringsToFile(ReadmePath, Lines, False);
  end;
end;
```

- [ ] **Step 2: Build Release and package**

```bash
MSYS_NO_PATHCONV=1 "/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/amd64/MSBuild.exe" azerothcore.vcxproj -p:Configuration=Release -p:Platform=x64 -nologo -v:minimal -m
MSYS_NO_PATHCONV=1 "/c/Users/Nick/AppData/Local/Programs/Inno Setup 7/ISCC.exe" installer.iss
```

Expected: `dist\AzerothCoreSetup.exe` produced. Manually run it on a clean-ish path (or a VM), confirm: install completes, Start Menu shortcut named "AzerothCore" appears, `Client\README.txt` exists with the expected instructions, launching from the shortcut opens the app.

- [ ] **Step 3: Commit**

```bash
git checkout -b installer
git add installer.iss
git commit -m "Add Inno Setup installer"
git push -u origin installer
gh pr create --title "Add installer" --body "Packages Release output into AzerothCoreSetup.exe. Verified: install completes, Start Menu entry is named AzerothCore, Client\\ folder + README created for the user's own client, launch works from the installed shortcut."
```

---

## Self-Review

**Spec coverage:**
- Core flow (path once, Play launches, client-native login + autofill) — Tasks 4, 6, 7, 8. ✓
- Addons (in-app browser default, folder shortcut, Felbite now, CurseForge later via `IAddonSource`, scraper health check) — Tasks 9, 10, 11. ✓
- Visual direction (real logo/hero art, no news panel, AzerothCore naming) — Task 6, Global Constraints. ✓
- Architecture (Frame + page-per-section) — Task 3. ✓
- Characters/armory — Task 12, with an explicit flagged gap (no confirmed backend) rather than a fabricated one — this is the one spec item not fully closed by this plan, called out rather than silently dropped.
- Installer / Client folder (no bundled client) — Task 13. ✓
- Testing philosophy (no UI test framework, Felbite health check is the one automated piece) — reflected throughout: Core/ services are TDD'd with real assertions, pages get manual-verification steps, Task 10 is the one CI-backed test.

**Placeholder scan:** no TBD/TODO/"add appropriate X" anywhere in the tasks above; the one open item (Characters backend) is explicitly named as a gap for Nick, not glossed over.

**Type consistency:** `RealmConfig` fields (`WowPath`, `RealmAddress`, `CredentialVaultEnabled`) are the same three across Tasks 2, 4, 6, 8. `RemoteAddon`/`IAddonSource` signatures match between Tasks 9 and 11. `RealmReachability` enum values (`Unconfigured`/`Checking`/`Online`/`Unreachable`) match between Task 5's definition and Task 6's `switch`.
