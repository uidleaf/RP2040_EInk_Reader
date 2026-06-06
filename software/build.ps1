<#
.SYNOPSIS
  RP2040 E-Ink Reader build & flash script
.DESCRIPTION
  Syncs sources to an ASCII-only build path, compiles with Pico SDK,
  generates UF2, and optionally flashes to RP2040.
.PARAMETER Clean
  Full rebuild (deletes build directory first).
.PARAMETER Flash
  After building, wait for RPI-RP2 drive and auto-flash.
.EXAMPLE
  .\build.ps1
  .\build.ps1 -Clean
  .\build.ps1 -Flash
#>
param([switch]$Clean, [switch]$Flash)

$ErrorActionPreference = "Stop"

# Use script location so Chinese path is never hardcoded
$script:ProjectDir = $PSScriptRoot
$script:BuildDir   = "C:\temp\eink"
$script:SdkPath    = "C:\temp\pico-sdk"

function info($msg)  { Write-Host "[INFO] $msg" -ForegroundColor Green }
function error($msg) { Write-Host "[ERROR] $msg" -ForegroundColor Red; exit 1 }

# ================================================================
# Check tools
# ================================================================
info "Checking tools..."
foreach ($cmd in @("arm-none-eabi-gcc", "cmake", "ninja", "python", "git")) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        error "$cmd not found in PATH"
    }
}
if (-not (Test-Path $SdkPath)) {
    error "Pico SDK not found: $SdkPath`nRun: git clone --depth 1 https://github.com/raspberrypi/pico-sdk.git $SdkPath`nThen: cd $SdkPath; git submodule update --init"
}
info "All tools ready"

# ================================================================
# Clean
# ================================================================
if ($Clean) {
    info "Cleaning build directory..."
    $b = Join-Path $BuildDir "build"
    if (Test-Path $b) { Remove-Item -Recurse -Force $b }
    info "Cleaned"
    if (-not $Flash) { exit 0 }
}

# ================================================================
# Sync sources (Chinese-path safe: use $PSScriptRoot)
# ================================================================
info "Syncing sources to $BuildDir ..."
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Get-ChildItem -Path $ProjectDir -File | Where-Object {
    $_.Extension -in ".c", ".h", ".txt", ".cmake"
} | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $BuildDir $_.Name) -Force
}
# Also copy CMakeLists.txt (no extension)
$cmakeFile = Join-Path $ProjectDir "CMakeLists.txt"
if (Test-Path $cmakeFile) { Copy-Item $cmakeFile $BuildDir -Force }

# Sync lib directory
$libSrc = Join-Path $ProjectDir "lib"
$libDst = Join-Path $BuildDir "lib"
if (Test-Path $libSrc) {
    robocopy $libSrc $libDst /e /njh /njs /ndl /nc | Out-Null
}
info "Sources synced"

# ================================================================
# CMake configure
# ================================================================
$buildDir = Join-Path $BuildDir "build"
$ninjaFile = Join-Path $buildDir "build.ninja"

if ((-not $Clean) -and (Test-Path $ninjaFile)) {
    info "CMake already configured"
} else {
    info "Running CMake configure..."
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    Push-Location $buildDir
    try {
        cmake -G "Ninja" "-DPICO_SDK_PATH=$SdkPath" "-DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON" $BuildDir
        if ($LASTEXITCODE -ne 0) { error "CMake failed" }
        info "CMake done"
    } finally { Pop-Location }
}

# ================================================================
# Ninja build
# ================================================================
info "Building..."
Push-Location $buildDir
try {
    ninja
    if ($LASTEXITCODE -ne 0) { error "Build failed" }
    info "Build done"
} finally { Pop-Location }

# ================================================================
# Generate UF2
# ================================================================
info "Generating UF2..."
Push-Location $buildDir
try {
    python -c @"
import struct, os
d = open('eink_reader.bin','rb').read()
t = (len(d) + 255) // 256
with open('eink_reader.uf2','wb') as f:
    for i in range(t):
        blk = bytearray(512)
        struct.pack_into('<8I', blk, 0,
            0x0A324655, 0x9E5D5157, 0x2000,
            0x10000000 + i*256, 256, i, t, 0xE48BFF56)
        s, e = i*256, min(i*256+256, len(d))
        blk[32:32+(e-s)] = d[s:e]
        struct.pack_into('<I', blk, 508, 0x0AB16F30)
        f.write(blk)
sz = os.path.getsize('eink_reader.uf2') / 1024
print(f'UF2: {t} blocks, {sz:.1f} KB')
"@
    $uf2Dst = Join-Path $ProjectDir "build\eink_reader.uf2"
    New-Item -ItemType Directory -Force -Path (Split-Path $uf2Dst) | Out-Null
    Copy-Item "eink_reader.uf2" $uf2Dst -Force
    info "UF2 -> $uf2Dst"
} finally { Pop-Location }

# ================================================================
# Flash (wait for RPI-RP2)
# ================================================================
if ($Flash) {
    info "Waiting for RP2040 BOOTSEL mode..."
    info "Hold BOOTSEL, plug USB, release BOOTSEL..."
    $timeout = 60
    1..$timeout | ForEach-Object {
        $drive = Get-WmiObject Win32_LogicalDisk |
            Where-Object { $_.VolumeName -eq "RPI-RP2" } |
            Select-Object -ExpandProperty DeviceID -ErrorAction SilentlyContinue
        if ($drive) {
            info "Found: $drive"
            Start-Sleep -Seconds 1
            Copy-Item (Join-Path $buildDir "eink_reader.uf2") "$drive\" -Force
            info "Flashed! RP2040 will reboot."
            exit 0
        }
        Start-Sleep -Seconds 1
    }
    error "Timeout: RPI-RP2 not detected"
}

info "============================================"
info "BUILD SUCCESS"
info "UF2: $ProjectDir\build\eink_reader.uf2"
info "Flash: .\build.ps1 -Flash"
info "============================================"
