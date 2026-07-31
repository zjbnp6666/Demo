#!/bin/bash
# MSYS2 MINGW64 一键构建脚本
# 用法: 在 MSYS2 MINGW64 终端中运行 ./build.sh

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build/msys2-debug"

# 确认在 MINGW64 环境中
if ! which gcc | grep -q mingw64; then
    echo "错误: 请在 MSYS2 MINGW64 终端中运行此脚本"
    echo "启动方式: 开始菜单 → MSYS2 → MINGW64"
    exit 1
fi

echo "=== CMake 配置 ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT_DIR" \
    -G "MinGW Makefiles" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=/mingw64 \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++

echo ""
echo "=== 编译 ==="
cmake --build . -j$(nproc)

echo ""
echo "=== 运行 ==="
echo "启动播放器..."
./QTffmpeg-Demo.exe
