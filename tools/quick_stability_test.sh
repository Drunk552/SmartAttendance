#!/bin/bash

# ==========================================
# 快速稳定性测试脚本 (10分钟版本)
# 用于开发期间快速验证
# ==========================================

set -e

APP="./build/attendance_app"
DURATION=600           # 10分钟测试
INTERVAL=5
LOG_FILE="quick_test.log"
PID_FILE="app.pid"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo -e "${GREEN}[INFO]${NC} 开始快速稳定性测试 (${DURATION}秒)"

# 检查应用程序
if [ ! -f "$APP" ]; then
    echo -e "${RED}[ERROR]${NC} 应用程序不存在: $APP"
    echo "请先构建项目: cd build && cmake .. && make"
    exit 1
fi

# 清理
rm -f "$LOG_FILE" "$PID_FILE"

# 启动应用程序
echo -e "${GREEN}[INFO]${NC} 启动应用程序..."
$APP > "$LOG_FILE" 2>&1 &
echo $! > "$PID_FILE"
APP_PID=$(cat "$PID_FILE")
echo -e "${GREEN}[INFO]${NC} 应用程序PID: $APP_PID"

# 等待初始化
sleep 3

# 检查进程
if ! ps -p $APP_PID > /dev/null; then
    echo -e "${RED}[ERROR]${NC} 应用程序启动失败"
    echo "查看日志: $LOG_FILE"
    exit 1
fi

# 获取初始内存
initial_rss=$(ps -o rss= -p $APP_PID | awk '{print $1}')
echo -e "${GREEN}[INFO]${NC} 初始RSS内存: ${initial_rss}KB"

# 监控循环
start_time=$(date +%s)
iterations=$((DURATION / INTERVAL))
crashed=false

for ((i=1; i<=iterations; i++)); do
    elapsed=$(( $(date +%s) - start_time ))
    
    if ! ps -p $APP_PID > /dev/null; then
        echo -e "${RED}[ERROR]${NC} 检测到程序崩溃！运行时间: ${elapsed}秒"
        crashed=true
        break
    fi
    
    # 每30秒输出一次状态
    if [ $((i % 6)) -eq 0 ]; then
        rss=$(ps -o rss= -p $APP_PID | awk '{print $1}')
        cpu=$(ps -o pcpu= -p $APP_PID | awk '{print $1}')
        echo -e "${GREEN}[INFO]${NC} 运行时间: ${elapsed}秒 | RSS: ${rss}KB | CPU: ${cpu}%"
    fi
    
    sleep $INTERVAL
done

# 清理
if ps -p $APP_PID > /dev/null; then
    echo -e "${GREEN}[INFO]${NC} 停止应用程序..."
    kill $APP_PID
    sleep 2
fi

rm -f "$PID_FILE"

# 报告结果
if [ "$crashed" = true ]; then
    echo -e "${RED}==========================================${NC}"
    echo -e "${RED}❌ 快速稳定性测试失败！程序崩溃${NC}"
    echo -e "${RED}==========================================${NC}"
    exit 1
else
    final_rss=$(ps -o rss= -p $APP_PID 2>/dev/null || echo "0")
    if [ "$final_rss" != "0" ]; then
        rss_increase=$(( (final_rss - initial_rss) * 100 / initial_rss ))
        echo -e "${GREEN}[INFO]${NC} 内存增长: ${rss_increase}%"
    fi
    
    echo -e "${GREEN}==========================================${NC}"
    echo -e "${GREEN}✅ 快速稳定性测试通过！${NC}"
    echo -e "${GREEN}✅ 连续运行 ${elapsed} 秒无崩溃${NC}"
    echo -e "${GREEN}==========================================${NC}"
fi

exit 0