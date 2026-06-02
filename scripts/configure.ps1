param(
    [string]$QtPrefix = "",

    [string]$BuildDir = "build",
    [ValidateSet("msvc", "mingw")]
    [string]$Toolchain = "msvc"
)

if ($Toolchain -eq "msvc") {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Error "Visual Studio Installer was not found. Install Visual Studio with the 'Desktop development with C++' workload, or run with -Toolchain mingw after installing MSYS2 Qt packages."
        exit 1
    }

    $installPath = & $vswhere -latest -products * -property installationPath
    if (-not $installPath) {
        Write-Error "No Visual Studio installation was found. Install Visual Studio with the 'Desktop development with C++' workload."
        exit 1
    }

    $vcToolsPath = Join-Path $installPath "VC\Tools\MSVC"
    if (-not (Test-Path $vcToolsPath)) {
        Write-Error "Visual Studio was found at '$installPath', but MSVC C++ tools are missing. Open Visual Studio Installer, choose Modify, and install 'Desktop development with C++' including 'MSVC v145' or the latest MSVC x64/x86 build tools."
        exit 1
    }

    $vsDevCmd = Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $vsDevCmd)) {
        Write-Error "Could not find VsDevCmd.bat in '$installPath'. Repair Visual Studio or install the C++ desktop workload."
        exit 1
    }

    if (-not $QtPrefix) {
        Write-Error "Pass -QtPrefix, for example: .\scripts\configure.ps1 -QtPrefix 'C:\Qt\6.8.0\msvc2022_64'"
        exit 1
    }

    if (-not (Test-Path $QtPrefix)) {
        Write-Error "QtPrefix '$QtPrefix' does not exist. Install the Qt MSVC package, then pass that exact folder, for example 'C:\Qt\6.11.1\msvc2022_64'."
        exit 1
    }

    if (-not (Test-Path (Join-Path $QtPrefix "lib\cmake\Qt6\Qt6Config.cmake"))) {
        Write-Error "QtPrefix '$QtPrefix' does not contain lib\cmake\Qt6\Qt6Config.cmake. For the MSVC path, install a Qt kit named like msvc2022_64, not mingw_64."
        exit 1
    }

    if ($QtPrefix -match "mingw|ucrt|msys") {
        Write-Error "QtPrefix '$QtPrefix' is a MinGW/MSYS Qt kit. The MSVC path needs a Qt MSVC kit such as C:\Qt\6.11.1\msvc2022_64. Use -Toolchain mingw only if you want the MinGW path."
        exit 1
    }

    $command = "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && cmake -S `"$PWD`" -B `"$BuildDir`" -G `"NMake Makefiles`" -DCMAKE_PREFIX_PATH=`"$QtPrefix`""
    cmd.exe /s /c $command
    exit $LASTEXITCODE
}

$mingwRoot = "C:\msys64\ucrt64"
if (-not (Test-Path "$mingwRoot\bin\g++.exe")) {
    Write-Error "MSYS2 UCRT64 g++ was not found at $mingwRoot\bin\g++.exe."
    exit 1
}

if (-not (Test-Path "$mingwRoot\lib\cmake\Qt6")) {
    Write-Error "MSYS2 Qt 6 was not found. Install it with: pacman -S mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-pdf"
    exit 1
}

cmake -S . -B $BuildDir `
    -G "MinGW Makefiles" `
    -DCMAKE_PREFIX_PATH="$mingwRoot" `
    -DCMAKE_C_COMPILER="$mingwRoot\bin\gcc.exe" `
    -DCMAKE_CXX_COMPILER="$mingwRoot\bin\g++.exe" `
    -DCMAKE_MAKE_PROGRAM="$mingwRoot\bin\mingw32-make.exe"
