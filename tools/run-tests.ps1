<#
.SYNOPSIS
    Builds and runs the Core unit tests.

.DESCRIPTION
    Each file in Core\*Tests.cpp is a standalone program with its own main()
    that asserts its way through one component. There is no test framework and
    they are deliberately not part of azerothcore.vcxproj: that project is a
    WinUI3 application and linking five extra main() functions into it is not
    possible. This script is the build definition for them.

    IMPORTANT: the tests signal failure with assert(), which compiles to
    nothing when NDEBUG is defined. Building these in a configuration that
    defines NDEBUG makes every test pass unconditionally while looking green,
    which is worse than having no tests at all. /UNDEBUG below is not
    decoration; do not remove it.

.PARAMETER OutputDir
    Where to put the built test executables. Defaults to a temp folder so the
    repo stays clean.
#>
[CmdletBinding()]
param(
    [string]$OutputDir = (Join-Path ([System.IO.Path]::GetTempPath()) 'ac-launcher-tests')
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

# Each test compiles against only the sources it exercises. Listing them
# explicitly rather than globbing Core\*.cpp keeps a failure pointing at one
# component instead of dragging the whole Core layer into every link.
# Libs are per-suite for the same reason: WowInstall reaches ShellExecuteEx and
# CredentialVault reaches SendInput, and linking those into every suite would
# hide which component actually depends on them.
$suites = @(
    @{ Name = 'WowInstall';        Test = 'Core\WowInstallTests.cpp';        Sources = @('Core\WowInstall.cpp');        Libs = @('shell32.lib') }
    @{ Name = 'RealmConfig';       Test = 'Core\RealmConfigTests.cpp';       Sources = @('Core\RealmConfig.cpp');       Libs = @() }
    @{ Name = 'RealmStatusChecker';Test = 'Core\RealmStatusCheckerTests.cpp';Sources = @('Core\RealmStatusChecker.cpp');Libs = @() }
    @{ Name = 'CredentialVault';   Test = 'Core\CredentialVaultTests.cpp';   Sources = @('Core\CredentialVault.cpp');   Libs = @('user32.lib') }
    @{ Name = 'FelbiteSource';     Test = 'Core\FelbiteSourceTests.cpp';     Sources = @('Core\FelbiteSource.cpp');     Libs = @() }
)

function Find-VcVars {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found at $vswhere" }

    # -latest alone can pick an install without the C++ toolset, so require the
    # VC tools component explicitly.
    $install = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $install) { throw 'No Visual Studio install with the C++ toolset was found.' }

    $vcvars = Join-Path $install 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $install" }
    return $vcvars
}

$vcvars = Find-VcVars
Write-Host "Toolchain: $vcvars"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$failed = @()
foreach ($suite in $suites) {
    $exe = Join-Path $OutputDir "$($suite.Name)Tests.exe"
    $inputs = (@($suite.Test) + $suite.Sources | ForEach-Object { '"' + (Join-Path $repoRoot $_) + '"' }) -join ' '

    # cl has to run inside the vcvars environment, and vcvars only sets that up
    # for the shell it is called from. Writing a .bat and invoking that is far
    # more robust than trying to nest the quoting through `cmd /c "... && ..."`,
    # which mangles the paths.
    $bat = Join-Path $OutputDir "build-$($suite.Name).bat"
    # vcvars64.bat shells out to vswhere.exe by bare name, so the VS Installer
    # directory has to be on PATH before it is called. It normally is, but not
    # when this script is reached through a shell that rewrote PATH (a Git Bash
    # parent will do exactly that), and the failure reads as
    # "'vswhere.exe' is not recognized" from inside vcvars rather than from here.
    $vsInstaller = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer'
    @(
        '@echo off'
        "set `"PATH=%SystemRoot%\system32;%SystemRoot%;$vsInstaller;%PATH%`""
        "call `"$vcvars`" >nul"
        # /Fo needs a trailing separator to mean "directory", but a lone
        # backslash before the closing quote escapes that quote and cl ends up
        # parsing the rest of the line as part of the path. Doubling it is the
        # fix: cl reads \\" as an escaped backslash followed by the quote.
        "cl /nologo /std:c++20 /EHsc /UNDEBUG /I`"$repoRoot`" $inputs /Fe:`"$exe`" /Fo:`"$OutputDir\\`" /link windowsapp.lib $($suite.Libs -join ' ')"
        'exit /b %ERRORLEVEL%'
    ) | Set-Content -Path $bat -Encoding ASCII

    $buildOutput = & cmd /c $bat 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "BUILD FAIL  $($suite.Name)" -ForegroundColor Red
        $buildOutput | ForEach-Object { Write-Host "    $_" }
        $failed += "$($suite.Name) (build)"
        continue
    }

    $runOutput = & $exe 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL        $($suite.Name)  (exit $LASTEXITCODE)" -ForegroundColor Red
        $runOutput | ForEach-Object { Write-Host "    $_" }
        $failed += $suite.Name
    }
    else {
        Write-Host "ok          $($suite.Name)" -ForegroundColor Green
    }
}

Write-Host ''
if ($failed.Count -gt 0) {
    Write-Host "$($failed.Count) of $($suites.Count) suites failed: $($failed -join ', ')" -ForegroundColor Red
    exit 1
}
Write-Host "All $($suites.Count) suites passed." -ForegroundColor Green
exit 0
