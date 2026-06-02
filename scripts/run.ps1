param(
    [string]$BuildDir = "build",
    [string]$Path = ""
)

$exePath = Join-Path $BuildDir "LaTeXApp.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "LaTeXApp.exe was not found at '$exePath'. Run .\scripts\build.ps1 first."
    exit 1
}

$platformPlugin = Join-Path $BuildDir "platforms\qwindowsd.dll"
$releasePlatformPlugin = Join-Path $BuildDir "platforms\qwindows.dll"
if (-not (Test-Path $platformPlugin) -and -not (Test-Path $releasePlatformPlugin)) {
    Write-Error "Qt runtime files were not found beside the app. Run .\scripts\deploy.ps1 first."
    exit 1
}

if ($Path) {
    Start-Process -FilePath (Resolve-Path $exePath) -WorkingDirectory (Resolve-Path $BuildDir) -ArgumentList "`"$Path`""
} else {
    Start-Process -FilePath (Resolve-Path $exePath) -WorkingDirectory (Resolve-Path $BuildDir)
}

