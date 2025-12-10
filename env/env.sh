#!/usr/bin/env bash

# 获取当前脚本所在的目录的上级目录，即项目根目录
# 这样无论你在哪 source，ROOT_DIR 都是正确的
if [[ -z "${ROOT_DIR+x}" ]]; then
  # 假设 env.sh 在 env/ 目录下，向上两级通常不安全，
  # 建议还是动态获取当前执行 pwd (前提是在根目录 source)
  export ROOT_DIR="$(pwd)"
fi

function echoc()
{
    echo -e "\e[0;91m$1\e[0m"
}

function envsetup()
{
    echo "---------------------------------------"
    echo "SmartAttendance 环境已加载!"
    echo "项目根目录: $ROOT_DIR"
    echo "可用指令:"
    echo "  m  / make   : 自动构建 (cmake + make)"
    echo "  r  / run    : 运行程序 (attendance_app)"
    echo "  cl / clean  : 清理构建目录"
    echo "  croot       : 回到项目根目录"
    echo "---------------------------------------"
}

# 快捷指令：回到根目录
function croot()
{
    cd "$ROOT_DIR" || return
}

# 快捷指令：清理
function clean()
{
    if [ -d "$ROOT_DIR/build" ]; then
        rm -rf "$ROOT_DIR/build"
        echo "Build directory cleaned."
    fi
}
# 别名
function make-distclean() { clean; }
function cl() { clean; }

# 核心指令：编译
function make()
{
    # 确保在根目录或 build 目录逻辑
    mkdir -p "$ROOT_DIR/build"
    cd "$ROOT_DIR/build" || return
    
    echo ">>> Running CMake..."
    cmake ..
    
    echo ">>> Running Make..."
    # 自动使用 nproc 获取核心数来加速编译
    command make -j$(nproc)
    
    # 编译完回到根目录 (可选，看个人习惯)
    cd "$ROOT_DIR" || return
}
# 别名 m
function m() { make; }

# 核心指令：运行
function run()
{
    cd "$ROOT_DIR/build"
    local EXE_PATH="$ROOT_DIR/build/attendance_app"
    
    if [ ! -f "$EXE_PATH" ]; then
        echoc "错误：找不到可执行文件 $EXE_PATH"
        echoc "请先执行 'make' 进行编译。"
        return
    fi
    
    echo ">>> Starting Application..."
    "$EXE_PATH"
}
# 别名 r
function r() { run; }

# 立即执行环境打印
envsetup