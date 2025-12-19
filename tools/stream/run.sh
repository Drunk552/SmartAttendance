#!/bin/bash
# Linux 版推流脚本：修复续行符语法，确保 ffmpeg 命令完整执行

# 检查 ffmpeg 是否安装
if ! command -v ffmpeg &> /dev/null; then
    echo "错误：ffmpeg 未安装！请先执行 sudo apt install -y ffmpeg"
    exit 1
fi

# 关键：ffmpeg 命令所有参数在同一逻辑行（用 \ 续行时，\ 后不能有任何字符，包括空格）
ffmpeg -f v4l2 \
       -video_size 640x480 \
       -framerate 30 \
       -pixel_format yuyv422 \
       -i /dev/video0 \
       -c:v libx264 \
       -preset ultrafast \
       -tune zerolatency \
       -b:v 1000k \
       -f flv rtmp://localhost/live/stream
