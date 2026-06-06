#!/usr/bin/env bash
# ================================================================
#  RP2040 墨水屏阅读器 — 一键编译脚本
#  用法: ./build.sh            # 增量编译
#        ./build.sh clean      # 完全重新编译
#        ./build.sh flash      # 编译后自动烧录 (需 RP2040 在 BOOTSEL 模式)
# ================================================================
set -e

# ---- 路径配置 (按需修改) ----
PROJECT_DIR="H:/file/墨水屏/RP2040"
BUILD_DIR="C:/temp/eink"
PICO_SDK_PATH="C:/temp/pico-sdk"

# ---- 颜色输出 ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
err()   { echo -e "${RED}[ERROR]${NC} $1"; }

# ---- 检查依赖 ----
check_deps() {
    info "检查编译工具链..."
    local missing=0

    for cmd in arm-none-eabi-gcc cmake ninja python git; do
        if ! command -v $cmd &>/dev/null; then
            err "$cmd 未安装或不在 PATH 中"
            missing=1
        fi
    done

    if [ ! -d "$PICO_SDK_PATH" ]; then
        err "Pico SDK 未找到: $PICO_SDK_PATH"
        err "请运行: git clone --depth 1 https://github.com/raspberrypi/pico-sdk.git $PICO_SDK_PATH"
        err "然后: cd $PICO_SDK_PATH && git submodule update --init"
        missing=1
    fi

    if [ $missing -ne 0 ]; then
        exit 1
    fi
    info "所有依赖就绪"
}

# ---- 同步源码 (中文路径 → 英文路径) ----
sync_sources() {
    info "同步源码到编译目录..."
    mkdir -p "$BUILD_DIR"

    cp "$PROJECT_DIR/main.c"          "$BUILD_DIR/main.c"
    cp "$PROJECT_DIR/ui.c"            "$BUILD_DIR/ui.c"
    cp "$PROJECT_DIR/ui.h"            "$BUILD_DIR/ui.h"
    cp "$PROJECT_DIR/EPD_2in9.c"      "$BUILD_DIR/EPD_2in9.c"
    cp "$PROJECT_DIR/EPD_2in9.h"      "$BUILD_DIR/EPD_2in9.h"
    cp "$PROJECT_DIR/DEV_Config.c"    "$BUILD_DIR/DEV_Config.c"
    cp "$PROJECT_DIR/DEV_Config.h"    "$BUILD_DIR/DEV_Config.h"
    cp "$PROJECT_DIR/hw_config.c"     "$BUILD_DIR/hw_config.c"
    cp "$PROJECT_DIR/font_gb2312.c"   "$BUILD_DIR/font_gb2312.c"
    cp "$PROJECT_DIR/font_gb2312.h"   "$BUILD_DIR/font_gb2312.h"
    cp "$PROJECT_DIR/CMakeLists.txt"  "$BUILD_DIR/CMakeLists.txt"
    cp "$PROJECT_DIR/pico_sdk_import.cmake" "$BUILD_DIR/pico_sdk_import.cmake"

    # 同步 lib 目录 (FatFs 等)
    if [ -d "$PROJECT_DIR/lib" ]; then
        cp -r "$PROJECT_DIR/lib" "$BUILD_DIR/lib"
    fi

    info "源码同步完成"
}

# ---- CMake 配置 ----
cmake_configure() {
    info "CMake 配置..."
    mkdir -p "$BUILD_DIR/build"
    cd "$BUILD_DIR/build"

    cmake -G "Ninja" \
        -DPICO_SDK_PATH="$PICO_SDK_PATH" \
        -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON \
        "$BUILD_DIR"

    info "CMake 配置完成"
}

# ---- Ninja 编译 ----
ninja_build() {
    info "开始编译..."
    cd "$BUILD_DIR/build"
    ninja
    info "编译完成"
}

# ---- 生成 UF2 ----
generate_uf2() {
    info "生成 UF2 文件..."
    cd "$BUILD_DIR/build"

    # picotool 在某些 Windows 上会崩溃，用 Python 备用方案
    python -c "
import struct, os

bin_path = 'eink_reader.bin'
if not os.path.exists(bin_path):
    print('ERROR: eink_reader.bin not found')
    exit(1)

with open(bin_path, 'rb') as f:
    data = f.read()

FAMILY_ID = 0xE48BFF56
MAGIC_S0  = 0x0A324655
MAGIC_S1  = 0x9E5D5157
MAGIC_END = 0x0AB16F30
PAYLOAD   = 256
FLASH     = 0x10000000

total = (len(data) + PAYLOAD - 1) // PAYLOAD

with open('eink_reader.uf2', 'wb') as f:
    for i in range(total):
        blk = bytearray(512)
        struct.pack_into('<I', blk,   0, MAGIC_S0)
        struct.pack_into('<I', blk,   4, MAGIC_S1)
        struct.pack_into('<I', blk,   8, 0x00002000)
        struct.pack_into('<I', blk,  12, FLASH + i * PAYLOAD)
        struct.pack_into('<I', blk,  16, PAYLOAD)
        struct.pack_into('<I', blk,  20, i)
        struct.pack_into('<I', blk,  24, total)
        struct.pack_into('<I', blk,  28, FAMILY_ID)
        s = i * PAYLOAD
        e = min(s + PAYLOAD, len(data))
        blk[32:32+(e-s)] = data[s:e]
        struct.pack_into('<I', blk, 508, MAGIC_END)
        f.write(blk)

kb = os.path.getsize('eink_reader.uf2') / 1024
print(f'UF2: {total} blocks, {kb:.1f} KB')
"

    # 复制回项目目录
    cp "$BUILD_DIR/build/eink_reader.uf2" "$PROJECT_DIR/build/eink_reader.uf2"
    info "UF2 已复制到 $PROJECT_DIR/build/eink_reader.uf2"
}

# ---- 烧录 (自动检测 RPI-RP2 磁盘) ----
flash_uf2() {
    info "等待 RP2040 进入 BOOTSEL 模式..."
    info "请按住 BOOTSEL 键，插入 USB，然后松开..."

    local timeout=30
    local elapsed=0
    local drive=""

    while [ $elapsed -lt $timeout ]; do
        # Windows: 查找卷标为 RPI-RP2 的磁盘
        drive=$(powershell.exe -Command "
            \$d = Get-WmiObject Win32_LogicalDisk | Where-Object { \$_.VolumeName -eq 'RPI-RP2' } | Select-Object -ExpandProperty DeviceID
            if (\$d) { Write-Output \$d }
        " 2>/dev/null | tr -d '\r\n ')

        if [ -n "$drive" ]; then
            info "检测到 RP2040: $drive"
            sleep 1  # 等待磁盘完全挂载
            cp "$BUILD_DIR/build/eink_reader.uf2" "${drive}/"
            info "烧录完成! RP2040 将自动重启"
            return 0
        fi

        sleep 0.5
        elapsed=$((elapsed + 1))
    done

    err "超时: 未检测到 RPI-RP2 磁盘"
    err "请确认 RP2040 已进入 BOOTSEL 模式后重试"
    return 1
}

# ---- 清理 ----
clean_build() {
    info "清理编译目录..."
    rm -rf "$BUILD_DIR/build"
    info "清理完成"
}

# ================================================================
# 主流程
# ================================================================
case "${1:-build}" in
    clean)
        clean_build
        ;;
    flash)
        check_deps
        sync_sources
        cmake_configure
        ninja_build
        generate_uf2
        flash_uf2
        ;;
    build|*)
        check_deps
        sync_sources

        # 如果 build 目录不存在或没有 ninja.build，则重新配置
        if [ ! -f "$BUILD_DIR/build/build.ninja" ]; then
            cmake_configure
        fi

        ninja_build
        generate_uf2

        info "============================================"
        info "编译成功!"
        info "UF2: $PROJECT_DIR/build/eink_reader.uf2"
        info "烧录: ./build.sh flash"
        info "============================================"
        ;;
esac
