param(
    [string]$BuildDir = "build",
    [ValidateSet("debug", "release")]
    [string]$Config = "debug",
    [string]$IsccPath = ""
)

$ErrorActionPreference = "Stop"

$buildRoot = Resolve-Path $BuildDir
$buildRootPath = $buildRoot.Path
$exePath = Join-Path $buildRootPath "LaTeXApp.exe"
$cachePath = Join-Path $buildRootPath "CMakeCache.txt"
$issPath = Join-Path $buildRootPath "installer\latexapp.iss"
$stageDir = Join-Path $buildRootPath "package"

if (-not (Test-Path $exePath)) {
    Write-Error "LaTeXApp.exe was not found at '$exePath'. Run .\scripts\build.ps1 first."
}

if (-not (Test-Path $cachePath)) {
    Write-Error "No CMake build tree was found at '$BuildDir'. Run .\scripts\configure.ps1 first."
}

if (-not (Test-Path $issPath)) {
    Write-Error "Generated Inno script was not found at '$issPath'. Re-run .\scripts\configure.ps1."
}

$prefixLine = Select-String -Path $cachePath -Pattern "^CMAKE_PREFIX_PATH:.*=(.+)$" | Select-Object -First 1
$qtPrefix = if ($prefixLine) { $prefixLine.Matches[0].Groups[1].Value } else { "" }
$deployTool = if ($qtPrefix) { Join-Path $qtPrefix "bin\windeployqt.exe" } else { "" }

if (-not (Test-Path $deployTool)) {
    Write-Error "windeployqt.exe was not found. Check CMAKE_PREFIX_PATH in '$cachePath'."
}

$resolvedStageParent = Resolve-Path (Split-Path -Parent $stageDir)
if (-not $stageDir.StartsWith($buildRootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "Refusing to recreate staging directory outside the build tree: '$stageDir'."
}

if (Test-Path $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
New-Item -Path $stageDir -ItemType Directory | Out-Null
Copy-Item -LiteralPath $exePath -Destination (Join-Path $stageDir "LaTeXApp.exe") -Force

$mode = if ($Config -eq "debug") { "--debug" } else { "--release" }
& $deployTool $mode (Join-Path $stageDir "LaTeXApp.exe")
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$templatesSource = Join-Path (Split-Path -Parent $PSScriptRoot) "templates"
if (Test-Path $templatesSource) {
    Copy-Item -LiteralPath $templatesSource -Destination (Join-Path $stageDir "templates") -Recurse -Force
}

if (-not $IsccPath) {
    $command = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($command) {
        $IsccPath = $command.Source
    } else {
        $candidates = @(
            "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
            "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
        )
        foreach ($candidate in $candidates) {
            if (Test-Path $candidate) {
                $IsccPath = $candidate
                break
            }
        }
    }
}

if (-not $IsccPath -or -not (Test-Path $IsccPath)) {
    Write-Error "ISCC.exe was not found. Install Inno Setup 6 or pass -IsccPath."
}

& $IsccPath $issPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Installer created under $(Join-Path $buildRootPath 'installer')."
