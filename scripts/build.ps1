param(
    [string]$BuildDir = "build",
    [string]$Config = ""
)

$cachePath = Join-Path $BuildDir "CMakeCache.txt"
if (-not (Test-Path $cachePath)) {
    Write-Error "No CMake build tree was found at '$BuildDir'. Run .\scripts\configure.ps1 first."
    exit 1
}

$generatorLine = Select-String -Path $cachePath -Pattern "^CMAKE_GENERATOR:INTERNAL=(.+)$" | Select-Object -First 1
$generator = if ($generatorLine) { $generatorLine.Matches[0].Groups[1].Value } else { "" }

if ($generator -eq "NMake Makefiles") {
    $qtDirLine = Select-String -Path $cachePath -Pattern "^Qt6_DIR:PATH=(.+)$" | Select-Object -First 1
    $qtDir = if ($qtDirLine) { $qtDirLine.Matches[0].Groups[1].Value } else { "" }
    if ($qtDir -match "msys64|mingw|ucrt64") {
        Write-Error "This MSVC build tree is using a MinGW/MSYS Qt package at '$qtDir'. Reconfigure with an MSVC Qt kit, for example: .\scripts\configure.ps1 -QtPrefix 'C:\Qt\6.11.1\msvc2022_64'. Use a fresh build directory or delete the old build folder first."
        exit 1
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Error "Visual Studio Installer was not found. Cannot enter the MSVC developer environment."
        exit 1
    }

    $installPath = & $vswhere -latest -products * -property installationPath
    if (-not $installPath) {
        Write-Error "No Visual Studio installation was found."
        exit 1
    }

    $vsDevCmd = Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $vsDevCmd)) {
        Write-Error "Could not find VsDevCmd.bat in '$installPath'."
        exit 1
    }

    $buildCommand = if ($Config) {
        "cmake --build `"$BuildDir`" --config `"$Config`""
    } else {
        "cmake --build `"$BuildDir`""
    }

    cmd.exe /s /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && $buildCommand"
    exit $LASTEXITCODE
}

if ($Config) {
    cmake --build $BuildDir --config $Config
} else {
    cmake --build $BuildDir
}
