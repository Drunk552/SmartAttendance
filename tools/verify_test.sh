#!/bin/bash

# ==========================================
# 验证测试脚本 - 测试系统基本稳定性
# 运行30秒验证系统是否正常启动和运行
# ==========================================

set -e

APP="./build/attendance_app"
DURATION=30
LOG_FILE="verify_test.log"
PID_FILE="verify_app.pid"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}==========================================${NC}"
echo -e "${BLUE}  智能考勤系统稳定性测试验证${NC}"
echo -e "${BLUE}==========================================${NC}"

# 检查应用程序
if [ ! -f "$APP" ]; then
    echo -e "${RED}[ERROR] 应用程序不存在: $APP${NC}"
    echo "请先构建项目:"
    echo "  cd build && cmake .. && make"
    exit 1
fi

echo -e "${GREEN}[INFO] 应用程序检查通过: $(basename $APP)${NC}"

# 清理旧文件
rm -f "$LOG_FILE" "$PID_FILE"

# 启动应用程序
echo -e "${GREEN}[INFO] 启动应用程序...${NC}"
$APP > "$LOG_FILE" 2>&1 &
APP_PID=$!
echo $APP_PID > "$PID_FILE"
echo -e "${GREEN}[INFO] 应用程序PID: $APP_PID${NC}"

# 等待初始化
echo -e "${GREEN}[INFO] 等待应用程序初始化 (5秒)...${NC}"
for i in {1..5}; do
    if ! ps -p $APP_PID > /dev/null; then
        echo -e "${RED}[ERROR] 应用程序在初始化期间退出${NC}"
        echo -e "${YELLOW}[LOG] 最后10行日志:${NC}"
        tail -10 "$LOG_FILE"
        exit 1
    fi
    echo -n "."
    sleep 1
done
echo ""
echo -e "${GREEN}[INFO] 应用程序初始化完成${NC}"

# 检查进程状态
if ! ps -p $APP_PID > /dev/null; then
    echo -e "${RED}[ERROR] 应用程序进程不存在${NC}"
    exit 1
fi

# 获取初始状态
initial_rss=$(ps -o rss= -p $APP_PID | awk '{print $1}')
initial_vsz=$(ps -o vsz= -p $APP_PID | awk '{print $1}')
echo -e "${GREEN}[INFO] 初始内存状态:${NC}"
echo -e "  RSS: ${initial_rss} KB"
echo -e "  VSZ: ${initial_vsz} KB"

# 运行监控
echo -e "${GREEN}[INFO] 开始${DURATION}秒运行测试...${NC}"
start_time=$(date +%s)
end_time=$((start_time + DURATION))
current_time=$start_time

while [ $current_time -lt $end_time ]; do
    elapsed=$((current_time - start_time))
    
    # 检查进程
    if ! ps -p $APP_PID > /dev/null; then
        echo -e "${RED}[ERROR] 检测到程序崩溃！运行时间: ${elapsed}秒${NC}"
        echo -e "${YELLOW}[LOG] 最后10行日志:${NC}"
        tail -10 "$LOG_FILE"
        exit 1
    fi
    
    # 每10秒输出状态
    if [ $((elapsed % 10)) -eq 0 ]; then
        rss=$(ps -o rss= -p $APP_PID | awk '{print $1}')
        cpu=$(ps -o pcpu= -p $APP_PID | awk '{print $1}')
        threads=$(ps -o thcount= -p $APP_PID | awk '{print $1}')
        echo -e "${GREEN}[INFO] 运行时间: ${elapsed}秒 | RSS: ${rss}KB | CPU: ${cpu}% | 线程: ${threads}${NC}"
    fi
    
    sleep 1
    current_time=$(date +%s)
done

# 测试完成
echo -e "${GREEN}[INFO] 测试完成，检查最终状态...${NC}"

# 获取最终状态
final_rss=$(ps -o rss= -p $APP_PID | awk '{print $1}')
final_vsz=$(ps -o vsz= -p $APP_PID | awk '{print $1}')
final_cpu=$(ps -o pcpu= -p $APP_PID | awk '{print $1}')

# 计算内存增长
rss_increase=$(( (final_rss - initial_rss) * 100 / initial_rss ))
vsz_increase=$(( (final_vsz - initial_vsz) * 100 / initial_vsz ))

echo -e "${GREEN}[INFO] 最终内存状态:${NC}"
echo -e "  RSS: ${final_rss} KB (增长: ${rss_increase}%)"
echo -e "  VSZ: ${final_vsz} KB (增长: ${vsz_increase}%)"
echo -e "  CPU: ${final_cpu}%"

# 停止应用程序
echo -e "${GREEN}[INFO] 停止应用程序...${NC}"
if ps -p $APP_PID > /dev/null; then
    kill $APP_PID
    sleep 2
    if ps -p $APP_PID > /dev/null; then
        echo -e "${YELLOW}[WARNING] 应用程序未正常退出，强制终止${NC}"
        kill -9 $APP_PID
    fi
fi

# 清理
rm -f "$PID_FILE"

# 生成验证报告
echo -e "${BLUE}==========================================${NC}"
echo -e "${BLUE}  验证测试报告${NC}"
echo -e "${BLUE}==========================================${NC}"

if [ $rss_increase -lt 50 ]; then
    echo -e "${GREEN}✅ 内存增长正常 (${rss_increase}% < 50%)${NC}"
else
    echo -e "${YELLOW}⚠️  内存增长较高 (${rss_increase}%)${NC}"
fi

if ps -p $APP_PID > /dev/null 2>&1; then
    echo -e "${RED}❌ 应用程序仍在运行 (清理失败)${NC}"
else
    echo -e "${GREEN}✅ 应用程序正常退出${NC}"
fi

echo -e "${GREEN}✅ 验证测试完成${NC}"
echo -e "${GREEN}✅ 连续运行 ${DURATION} 秒无崩溃${NC}"
echo -e "${GREEN}✅ 系统基本稳定性验证通过${NC}"

echo -e "${BLUE}==========================================${NC}"
echo -e "${GREEN}下一步:${NC}"
echo -e "1. 运行完整测试: ${YELLOW}make stability_test${NC} (1小时)"
echo -e "2. 运行快速测试: ${YELLOW}make quick_stability_test${NC} (10分钟)"
echo -e "3. 分析结果: ${YELLOW}make analyze_stability${NC}"
echo -e "${BLUE}==========================================${NC}"

exit 0