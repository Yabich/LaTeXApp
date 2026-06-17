param(
    [string]$QtPrefix = "",
    [string]$BuildDir = "build",
    [string]$IsccPath = "",
    [switch]$SkipConfigure,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

if (-not $SkipConfigure) {
    if (-not $QtPrefix) {
        Write-Error "Pass -QtPrefix, for example: .\scripts\release.ps1 -QtPrefix 'C:\Qt\6.11.1\msvc2022_64'"
    }

    & "$PSScriptRoot\configure.ps1" -QtPrefix $QtPrefix -BuildDir $BuildDir -BuildType Release
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

& "$PSScriptRoot\build.ps1" -BuildDir $BuildDir
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not $SkipTests) {
    & "$PSScriptRoot\test.ps1" -BuildDir $BuildDir
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

& "$PSScriptRoot\make-installer.ps1" -BuildDir $BuildDir -Config release -IsccPath $IsccPath
exit $LASTEXITCODE
