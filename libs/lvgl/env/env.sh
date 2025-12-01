#!/usr/bin/env bash
#
shopt -s extglob
# ——— 只在第一次 source 时定义 ROOT_DIR —— #
ROOT_DIR="$(pwd)"

function echoc()
{
    echo -e "\e[0;91m$1\e[0m"
}

# repo-tools.sh —— 包含两大功能：
# 1. repo_sync_all [并发数]        并发同步所有有权限的仓库
# 2. repo_init_and_sync [并发数]   交互式选择 manifest，一键 init + 并发 sync
# —— 并发同步函数（不变）
function resync() {
  if [ "$PWD" != "$ROOT_DIR" ]; then
    echo "错误：请在根目录 $ROOT_DIR 下执行此脚本，当前在 $PWD" >&2
    return
  fi
  repo sync -j4
}


function repo-clean(){
  if [ "$PWD" != "$ROOT_DIR" ]; then
    echo "错误：请在根目录 $ROOT_DIR 下执行此脚本，当前在 $PWD" >&2
    return
  fi
  rm -rf ./.repo
  rm -rf -- !(env)/
  echo "#############清除所有仓库##############"
}

function envsetup()
{
    echo "OK!!! env start ..."
}
function make()
{
    if [ "$PWD" != "$ROOT_DIR" ]; then
    echo "错误：请在根目录 $ROOT_DIR 下执行此脚本，当前在 $PWD" >&2
    return
    fi
    mkdir -p $ROOT_DIR/build
    cd build
    command cmake ..
    command make -j16
    cd $ROOT_DIR
}
function make-distclean()
{
    if [ "$PWD" != "$ROOT_DIR" ]; then
    echo "错误：请在根目录 $ROOT_DIR 下执行此脚本，当前在 $PWD" >&2
    return
    fi
    rm $ROOT_DIR/build -rf
}
function run()
{
    if [ "$PWD" != "$ROOT_DIR" ]; then
    echo "错误：请在根目录 $ROOT_DIR 下执行此脚本，当前在 $PWD" >&2
    return
    fi
    $ROOT_DIR/build/app
}
declare -f croot > /dev/null || envsetup 