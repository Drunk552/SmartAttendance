# 友好考勤软件系统

该工程为wsl2系统下lvgl模拟环境工程，该文档主要是用于相关配套环境安装和工程编译。

## 1.wsl2环境安装

wsl2环境安装请参看wsl2环境安装相关文档。

## 2.依赖安装

```shell
#更新
sudo apt update
#安装依赖
sudo apt install -y build-essential cmake git pkg-config libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev
#安装freetype
sudo apt install -y libfreetype6-dev
#安装字体
sudo apt install -y fonts-noto-cjk

```

## 3.项目根目录启动编译脚本

```shell
source env/env.sh
```



## 4.编译

```shell
make
```

## 5.运行

```shell
run
```

## 6.若要清除编译工程

```shell
make-distclean
```

