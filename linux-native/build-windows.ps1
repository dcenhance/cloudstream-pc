param(
    [string]$ProviderHostInstall = '',
    [string[]]$AdditionalRuntimeDlls = @(),
    [string]$JavaRuntime = $env:JAVA_HOME,
    [string]$FfmpegExecutable = '',
    [string]$SoftwareOpenGLDirectory = '',
    [string]$MsvcRuntimeDirectory = '',
    [string]$ThirdPartyNoticesDirectory = ''
)
$ErrorActionPreference = 'Stop'

# Build CloudStream for Windows with Qt 6 + MSVC.
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Native = $PSScriptRoot
$Build = Join-Path $Native 'build-windows'
$Dist = Join-Path $Root 'dist-windows'
if ([string]::IsNullOrWhiteSpace($MsvcRuntimeDirectory) -and $env:VCToolsRedistDir) {
    $MsvcRuntimeDirectory = Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC143.CRT'
}
foreach ($dll in @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
    if ([string]::IsNullOrWhiteSpace($MsvcRuntimeDirectory) -or
        -not (Test-Path (Join-Path $MsvcRuntimeDirectory $dll))) {
        throw "Set MsvcRuntimeDirectory to the Visual Studio x64 Microsoft.VC143.CRT redistributable directory; missing $dll."
    }
}
if ([string]::IsNullOrWhiteSpace($JavaRuntime) -or
    -not (Test-Path (Join-Path $JavaRuntime 'bin\java.exe'))) {
    throw 'JavaRuntime (or JAVA_HOME) must contain a Windows Java 17 runtime to bundle.'
}

foreach ($name in @('qmake', 'nmake', 'windeployqt', 'java')) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "$name was not found in PATH. Install Qt 6 (MSVC), Java 17, and add their bin directories to PATH."
    }
}
foreach ($name in @('MPV_INCLUDE', 'SDL2_INCLUDE', 'MPV_LIB', 'SDL2_LIB', 'MPV_DLL', 'SDL2_DLL')) {
    if ([string]::IsNullOrWhiteSpace((Get-Item "Env:$name" -ErrorAction SilentlyContinue).Value)) {
        throw "Set $name before building. MPV_LIB and SDL2_LIB must point to Windows .lib files."
    }
}

if ([string]::IsNullOrWhiteSpace($ProviderHostInstall)) {
    Push-Location $Root
    try {
        & .\gradlew.bat :provider-host:installDist
        if ($LASTEXITCODE -ne 0) { throw 'Provider host build failed.' }
    } finally { Pop-Location }
    $ProviderHostInstall = Join-Path $Root 'provider-host\build\install\cloudstream-provider-host'
}
if (-not (Test-Path (Join-Path $ProviderHostInstall 'lib')) -or
    -not (Test-Path (Join-Path $ProviderHostInstall 'bin\cloudstream-provider-host.bat'))) {
    throw 'ProviderHostInstall must point to a complete Gradle installDist distribution.'
}

New-Item -ItemType Directory -Force $Build | Out-Null
Push-Location $Build
try {
    & qmake (Join-Path $Native 'cloudstream-linux.pro') CONFIG+=release
    if ($LASTEXITCODE -ne 0) { throw 'qmake failed.' }
    & nmake
    if ($LASTEXITCODE -ne 0) { throw 'nmake failed.' }
} finally { Pop-Location }

if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
New-Item -ItemType Directory -Force $Dist | Out-Null
Copy-Item (Join-Path $Build 'release\cloudstream.exe') $Dist
& windeployqt --release --no-translations (Join-Path $Dist 'cloudstream.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed.' }

Copy-Item -Recurse $ProviderHostInstall (Join-Path $Dist 'provider-host')
Copy-Item $env:MPV_DLL $Dist -ErrorAction Stop
Copy-Item $env:SDL2_DLL $Dist -ErrorAction Stop
foreach ($dll in $AdditionalRuntimeDlls) {
    Copy-Item $dll $Dist -ErrorAction Stop
}
Copy-Item -Recurse $JavaRuntime (Join-Path $Dist 'runtime') -ErrorAction Stop
Copy-Item (Join-Path $MsvcRuntimeDirectory '*.dll') $Dist -ErrorAction Stop
Copy-Item (Join-Path $Root 'LICENSE') $Dist -ErrorAction Stop
if (-not [string]::IsNullOrWhiteSpace($ThirdPartyNoticesDirectory)) {
    Copy-Item -Recurse $ThirdPartyNoticesDirectory (Join-Path $Dist 'licenses') -ErrorAction Stop
}
if (-not [string]::IsNullOrWhiteSpace($FfmpegExecutable)) {
    Copy-Item $FfmpegExecutable (Join-Path $Dist 'ffmpeg.exe') -ErrorAction Stop
}
if (-not [string]::IsNullOrWhiteSpace($SoftwareOpenGLDirectory)) {
    # A recent Mesa fallback is required for current libmpv on GPU-less VMs.
    Copy-Item (Join-Path $SoftwareOpenGLDirectory 'opengl32.dll') (Join-Path $Dist 'opengl32sw.dll') -Force
    Copy-Item (Join-Path $SoftwareOpenGLDirectory 'libgallium_wgl.dll') $Dist -Force
}

Write-Output "Windows package: $Dist"
