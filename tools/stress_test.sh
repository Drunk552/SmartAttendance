#!/bin/bash
APP="./build/attendance_app"
# 启动应用
$APP &
PID=$!
echo "App started with PID: $PID"

# 每5秒记录一次内存，持续1小时(3600秒)
for i in {1..720}; do
    if ! ps -p $PID > /dev/null; then
        echo "Crash detected!"
        exit 1
    fi
    # 打印 RSS (物理内存占用)
    ps -o rss,pmem -p $PID
    sleep 5
done
echo "Stress test passed."
kill $PID
