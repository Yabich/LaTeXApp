param(
    [string]$BuildDir = "build",
    [ValidateSet("debug", "release")]
    [string]$Config = "debug"
)

$exePath = Join-Path $BuildDir "LaTeXApp.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "LaTeXApp.exe was not found at '$exePath'. Run .\scripts\build.ps1 first."
    exit 1
}

$cachePath = Join-Path $BuildDir "CMakeCache.txt"
if (-not (Test-Path $cachePath)) {
    Write-Error "No CMake build tree was found at '$BuildDir'. Run .\scripts\configure.ps1 first."
    exit 1
}

$prefixLine = Select-String -Path $cachePath -Pattern "^CMAKE_PREFIX_PATH:.*=(.+)$" | Select-Object -First 1
$qtPrefix = if ($prefixLine) { $prefixLine.Matches[0].Groups[1].Value } else { "" }
$deployTool = if ($qtPrefix) { Join-Path $qtPrefix "bin\windeployqt.exe" } else { "" }

if (-not (Test-Path $deployTool)) {
    Write-Error "windeployqt.exe was not found. Check CMAKE_PREFIX_PATH in '$cachePath'."
    exit 1
}

$mode = if ($Config -eq "debug") { "--debug" } else { "--release" }
& $deployTool $mode $exePath
exit $LASTEXITCODE

