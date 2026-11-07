# Builds a separate Windows x64 OpenRGB package on GitHub Actions.
# Uses the official Qt 6.8.3 MSVC 2022 archive from download.qt.io,
# official jom, and the Visual Studio 2022 toolchain on the runner.
# Does not launch OpenRGB.exe and refuses to run outside this fork's Actions job.

$ErrorActionPreference = "Stop"

if ($env:GITHUB_ACTIONS -ne "true") {
    throw "This script runs only inside GitHub Actions. It does not build on a local PC."
}
if ($env:GITHUB_REPOSITORY -ne "allenjacobg-hue/OpenRGB") {
    throw "Refusing to run outside allenjacobg-hue/OpenRGB."
}

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$File,
        [Parameter(ValueFromRemainingArguments = $true)][string[]]$CommandArgs
    )
    & $File @CommandArgs
    if ($LASTEXITCODE -ne 0) {
        throw "$File exited with code $LASTEXITCODE"
    }
}

function Import-VcVars {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe is not on this runner."
    }
    $script:VsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $script:VsInstall) {
        throw "Visual Studio with the MSVC x64 toolset is not installed on this runner."
    }
    $vcvars = Join-Path $script:VsInstall "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvars)) {
        throw "vcvarsall.bat was not found at $vcvars"
    }
    Write-Host "Using Visual Studio at $script:VsInstall"
    $lines = & cmd.exe /c "`"$vcvars`" x64 >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "vcvarsall.bat x64 failed with exit code $LASTEXITCODE"
    }
    foreach ($line in $lines) {
        if ($line -match '^([A-Za-z_][A-Za-z0-9_]*)=(.*)$') {
            Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
        }
    }
}

function Get-PeInfo {
    param([Parameter(Mandatory = $true)][string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $reader = New-Object System.IO.BinaryReader $stream
        $stream.Seek(0x3C, [System.IO.SeekOrigin]::Begin) | Out-Null
        $peOffset = $reader.ReadInt32()
        $stream.Seek($peOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $signature = $reader.ReadUInt32()
        $machine = $reader.ReadUInt16()
        $stream.Seek($peOffset + 24, [System.IO.SeekOrigin]::Begin) | Out-Null
        $magic = $reader.ReadUInt16()
        $stream.Seek($peOffset + 24 + 68, [System.IO.SeekOrigin]::Begin) | Out-Null
        $subsystem = $reader.ReadUInt16()
        return [pscustomobject]@{
            Signature = $signature
            Machine   = $machine
            Magic     = $magic
            Subsystem = $subsystem
        }
    }
    finally {
        $stream.Dispose()
    }
}

Write-Host "Installing official Qt 6.8.3 from download.qt.io"
python -m pip install --disable-pip-version-check "aqtinstall==3.3.0"
if ($LASTEXITCODE -ne 0) { throw "pip install aqtinstall failed" }
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir C:\Qt -m qt5compat
if ($LASTEXITCODE -ne 0) { throw "aqt install-qt failed" }

$QtBin = "C:\Qt\6.8.3\msvc2022_64\bin"
$Qmake = Join-Path $QtBin "qmake.exe"
if (-not (Test-Path $Qmake)) {
    throw "Official qmake.exe was not installed at $Qmake"
}

Write-Host "Installing official jom from download.qt.io"
$JomZip = Join-Path $env:RUNNER_TEMP "jom.zip"
Invoke-WebRequest -Uri "https://download.qt.io/official_releases/jom/jom_1_1_7.zip" -OutFile $JomZip
New-Item -ItemType Directory -Force -Path "C:\Qt\jom" | Out-Null
Expand-Archive -Path $JomZip -DestinationPath "C:\Qt\jom" -Force
if (-not (Test-Path "C:\Qt\jom\jom.exe")) {
    throw "jom.exe was not in the official jom archive"
}

Import-VcVars
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;C:\Qt\jom;" + $env:PATH

$QtVersion = & $Qmake -v
if ($LASTEXITCODE -ne 0) { throw "qmake -v failed" }
Write-Host $QtVersion
if ($QtVersion -notmatch "6\.8\.3") {
    throw "qmake is not Qt 6.8.3"
}

$ClVersion = & cl 2>&1 | Select-Object -First 1
Write-Host $ClVersion

Write-Host "Compiling the mock HID enumeration test"
$TestExe = Join-Path $env:RUNNER_TEMP "hid_safe_mode_enumeration_test.exe"
& cl /nologo /std:c++17 /EHsc /utf-8 /W3 /DWIN32 /D_WIN32 /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX `
    /I. /Idependencies\json /Idependencies\hidapi-hotplug-win\include /Ihidapi_wrapper /Ii2c_smbus /IRGBController /ISPDAccessor `
    tests\hid_safe_mode\hid_safe_mode_enumeration_test.cpp HIDSafeModeScan.cpp `
    /Fe:$TestExe
if ($LASTEXITCODE -ne 0) { throw "mock HID test compile failed" }

$TestLog = Join-Path $env:RUNNER_TEMP "hid-safe-mode-test.txt"
$Template = Join-Path $Root "experiment\OpenRGB.detectors.template.json"
& cmd.exe /c "`"$TestExe`" `"$Template`" > `"$TestLog`" 2>&1"
Get-Content $TestLog | Write-Host
if ($LASTEXITCODE -ne 0) {
    throw "mock HID test failed"
}
Write-Host "Mock HID test passed. OpenRGB.exe was not started."

Write-Host "Configuring OpenRGB with the baseline qmake release settings"
Invoke-Checked $Qmake "OpenRGB.pro" "CONFIG-=debug_and_release" "CONFIG+=release"
Write-Host "Compiling with jom"
Invoke-Checked "C:\Qt\jom\jom.exe"

$Release = Join-Path $Root "release"
Invoke-Checked (Join-Path $QtBin "windeployqt.exe") `
    "--no-patchqt" "--no-quick-import" "--no-translations" "--no-system-d3d-compiler" `
    "--no-compiler-runtime" "--no-opengl-sw" "--no-network" $Release

foreach ($dll in @("Qt6Core5Compat.dll", "Qt6OpenGL.dll")) {
    $source = Join-Path $QtBin $dll
    if (-not (Test-Path $source)) { throw "Official Qt DLL missing: $source" }
    Copy-Item $source $Release -Force
}

$crt = Get-ChildItem -Path $env:VCToolsRedistDir -Recurse -Filter "vcruntime140.dll" |
    Where-Object { $_.FullName -match '\\x64\\' } |
    Select-Object -First 1
if (-not $crt) { throw "MSVC x64 vcruntime140.dll was not found in VCToolsRedistDir" }
$crtDir = $crt.DirectoryName
Write-Host "Copying MSVC CRT DLLs from $crtDir"
Copy-Item (Join-Path $crtDir "*.dll") $Release -Force

$OutDir = Join-Path $Root "OpenRGB Windows 64-bit"
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
Move-Item $Release $OutDir

$Exe = Join-Path $OutDir "OpenRGB.exe"
if (-not (Test-Path $Exe)) { throw "OpenRGB.exe was not produced" }
$pe = Get-PeInfo $Exe
Write-Host ("PE signature=0x{0:X8} machine=0x{1:X4} magic=0x{2:X4} subsystem={3}" -f $pe.Signature, $pe.Machine, $pe.Magic, $pe.Subsystem)
if ($pe.Signature -ne 0x4550) { throw "OpenRGB.exe is not a PE file" }
if ($pe.Machine -ne 0x8664) { throw "OpenRGB.exe is not x64" }
if ($pe.Magic -ne 0x20B) { throw "OpenRGB.exe is not PE32+" }

$required = @(
    "OpenRGB.exe",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "hidapi-hotplug.dll",
    "libusb-1.0.dll",
    "vcruntime140.dll",
    "platforms\qwindows.dll"
)
foreach ($relative in $required) {
    $path = Join-Path $OutDir $relative
    if (-not (Test-Path $path)) { throw "Required package file missing: $relative" }
}

$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
$dependents = @()
if ($dumpbin) {
    $dependents = & dumpbin.exe /dependents $Exe
    $dependents | Write-Host
    $system = @(
        "kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
        "ole32.dll", "oleaut32.dll", "comdlg32.dll", "ws2_32.dll", "iphlpapi.dll",
        "setupapi.dll", "hid.dll", "cfgmgr32.dll", "ntdll.dll", "dwmapi.dll",
        "uxtheme.dll", "imm32.dll", "winmm.dll", "version.dll", "bcrypt.dll",
        "crypt32.dll", "shlwapi.dll", "comctl32.dll", "gdiplus.dll", "opengl32.dll",
        "d3d11.dll", "dxgi.dll", "msvcrt.dll", "ucrtbase.dll", "secur32.dll",
        "rpcrt4.dll", "oleacc.dll", "propsys.dll", "netapi32.dll", "wtsapi32.dll",
        "userenv.dll", "authz.dll", "mpr.dll", "dnsapi.dll", "nsi.dll"
    )
    foreach ($line in $dependents) {
        $trim = $line.Trim()
        if ($trim -notmatch '(?i)\.dll$') { continue }
        if ($trim -match '(?i)^api-ms-win-') { continue }
        if ($system -contains $trim.ToLower()) { continue }
        $found = Get-ChildItem $OutDir -Filter $trim -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $found) { throw "Imported DLL is not in the package: $trim" }
    }
}

$stageParent = Join-Path $Root "dist"
$stage = Join-Path $stageParent "OpenRGB-windows-x64-hid-safe"
if (Test-Path $stageParent) { Remove-Item $stageParent -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item $OutDir (Join-Path $stage "OpenRGB Windows 64-bit") -Recurse
New-Item -ItemType Directory -Force -Path (Join-Path $stage "separate-config") | Out-Null
Copy-Item (Join-Path $Root "experiment\OpenRGB.detectors.template.json") (Join-Path $stage "separate-config\OpenRGB.json")
Copy-Item (Join-Path $Root "LICENSE") $stage
Copy-Item (Join-Path $Root "experiment\PROVENANCE.md") $stage
Copy-Item (Join-Path $Root "experiment\WINDOWS_ARTIFACT_README.txt") (Join-Path $stage "README.txt")
Copy-Item $TestLog (Join-Path $stage "mock-hid-test.txt")

$commit = (git rev-parse HEAD).Trim()
$commitDate = (git log -n 1 --pretty=format:%cI).Trim()
$info = @(
    "repository=allenjacobg-hue/OpenRGB",
    "commit=$commit",
    "commit_date=$commitDate",
    "parent_baseline=8e16de27858cf2be6af54a33ce003df74cb9fd3e",
    "qt=$($QtVersion -replace '\s+', ' ')",
    "compiler=$ClVersion",
    "visual_studio=$script:VsInstall",
    "pe_machine=0x8664",
    "pe_magic=0x20B",
    "openrgb_exe_launched=false",
    "hardware_scan=false",
    "lighting_write=false"
)
$info | Set-Content -Encoding ascii (Join-Path $stage "BUILD-INFO.txt")
if ($dependents.Count -gt 0) {
    $dependents | Set-Content -Encoding ascii (Join-Path $stage "OpenRGB.exe.dependents.txt")
}

$hashes = Get-ChildItem $stage -Recurse -File | ForEach-Object {
    $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower()
    $relative = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
    "{0}  {1}" -f $hash, $relative
}
$hashes | Set-Content -Encoding ascii (Join-Path $stage "FILE-SHA256.txt")

$zip = Join-Path $stageParent "OpenRGB-windows-x64-hid-safe.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
& tar.exe -a -c -f $zip -C $stageParent "OpenRGB-windows-x64-hid-safe"
if ($LASTEXITCODE -ne 0) { throw "tar zip failed" }
$zipHash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
$zipName = Split-Path $zip -Leaf
"{0}  {1}" -f $zipHash, $zipName | Set-Content -Encoding ascii (Join-Path $stageParent "OpenRGB-windows-x64-hid-safe.zip.sha256")
Write-Host "ZIP_SHA256=$zipHash"
Write-Host "Package directory listing:"
Get-ChildItem (Join-Path $stage "OpenRGB Windows 64-bit") | Select-Object Name, Length | Format-Table | Out-String | Write-Host
Write-Host "Done. OpenRGB.exe was not executed."
