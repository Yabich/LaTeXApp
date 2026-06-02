param(
    [string]$BuildDir = "build"
)

$cachePath = Join-Path $BuildDir "CMakeCache.txt"
if (-not (Test-Path $cachePath)) {
    Write-Error "No CMake build tree was found at '$BuildDir'. Run .\scripts\configure.ps1 first."
    exit 1
}

$prefixLine = Select-String -Path $cachePath -Pattern "^CMAKE_PREFIX_PATH:.*=(.+)$" | Select-Object -First 1
$qtPrefix = if ($prefixLine) { $prefixLine.Matches[0].Groups[1].Value } else { "" }
$qtBin = if ($qtPrefix) { Join-Path $qtPrefix "bin" } else { "" }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installPath = if (Test-Path $vswhere) { & $vswhere -latest -products * -property installationPath } else { "" }
$vsDevCmd = if ($installPath) { Join-Path $installPath "Common7\Tools\VsDevCmd.bat" } else { "" }

if (Test-Path $vsDevCmd) {
    $pathPrefix = if (Test-Path $qtBin) { "set PATH=$qtBin;%PATH% && " } else { "" }
    cmd.exe /s /c "$pathPrefix call `"$vsDevCmd`" -arch=x64 -host_arch=x64 && ctest --test-dir `"$BuildDir`" --output-on-failure"
    exit $LASTEXITCODE
}

if (Test-Path $qtBin) {
    $env:PATH = "$qtBin;$env:PATH"
}
ctest --test-dir $BuildDir --output-on-failure

