#!/bin/bash

# ==========================================
# 智能考勤系统稳定性测试脚本
# 测试要求：连续运行1小时无崩溃、无显著内存上涨
# ==========================================

set -e

# 配置参数
APP="./build/attendance_app"
DURATION=3600          # 测试持续时间（秒）
INTERVAL=5             # 监控间隔（秒）
LOG_FILE="stability_test.log"
CSV_FILE="memory_usage.csv"
MAX_MEMORY_INCREASE_PERCENT=20  # 最大允许内存增长百分比

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 初始化
print_info "开始智能考勤系统稳定性测试"
print_info "测试时长: $((DURATION/60)) 分钟"
print_info "监控间隔: ${INTERVAL} 秒"
print_info "最大允许内存增长: ${MAX_MEMORY_INCREASE_PERCENT}%"

# 检查应用程序是否存在
if [ ! -f "$APP" ]; then
    print_error "应用程序不存在: $APP"
    print_error "请先构建项目: cd build && cmake .. && make"
    exit 1
fi

# 清理旧的日志文件
rm -f "$LOG_FILE" "$CSV_FILE"

# 创建CSV文件头
echo "timestamp,elapsed_seconds,pid,rss_kb,vsz_kb,pmem,cpu_percent,threads" > "$CSV_FILE"

# 启动应用程序
print_info "启动应用程序: $APP"
$APP > "$LOG_FILE" 2>&1 &
APP_PID=$!
print_info "应用程序PID: $APP_PID"

# 等待应用程序初始化
sleep 3

# 检查进程是否仍在运行
if ! ps -p $APP_PID > /dev/null; then
    print_error "应用程序启动后立即退出，请检查日志: $LOG_FILE"
    exit 1
fi

print_info "应用程序启动成功，开始监控..."

# 获取初始内存使用情况
initial_rss=$(ps -o rss= -p $APP_PID | awk '{print $1}')
initial_vsz=$(ps -o vsz= -p $APP_PID | awk '{print $1}')
max_rss=$initial_rss
max_vsz=$initial_vsz

print_info "初始内存使用 - RSS: ${initial_rss}KB, VSZ: ${initial_vsz}KB"

# 计算监控次数
iterations=$((DURATION / INTERVAL))
start_time=$(date +%s)

# 主监控循环
for ((i=1; i<=iterations; i++)); do
    current_time=$(date +%s)
    elapsed=$((current_time - start_time))
    
    # 检查进程是否仍在运行
    if ! ps -p $APP_PID > /dev/null; then
        print_error "检测到程序崩溃！已运行时间: ${elapsed} 秒"
        print_error "请查看日志文件: $LOG_FILE"
        exit 1
    fi
    
    # 获取进程状态
    ps_output=$(ps -o pid,rss,vsz,pmem,pcpu,thcount -p $APP_PID | tail -1)
    pid=$(echo $ps_output | awk '{print $1}')
    rss=$(echo $ps_output | awk '{print $2}')
    vsz=$(echo $ps_output | awk '{print $3}')
    pmem=$(echo $ps_output | awk '{print $4}')
    pcpu=$(echo $ps_output | awk '{print $5}')
    threads=$(echo $ps_output | awk '{print $6}')
    
    # 更新最大内存使用
    if [ $rss -gt $max_rss ]; then
        max_rss=$rss
    fi
    if [ $vsz -gt $max_vsz ]; then
        max_vsz=$vsz
    fi
    
    # 计算内存增长百分比
    rss_increase_percent=$(( (rss - initial_rss) * 100 / initial_rss ))
    
    # 记录到CSV
    timestamp=$(date +"%Y-%m-%d %H:%M:%S")
    echo "$timestamp,$elapsed,$pid,$rss,$vsz,$pmem,$pcpu,$threads" >> "$CSV_FILE"
    
    # 每30次输出一次状态
    if [ $((i % 30)) -eq 0 ]; then
        print_info "运行时间: ${elapsed}秒 | RSS: ${rss}KB (+${rss_increase_percent}%) | CPU: ${pcpu}% | 线程数: ${threads}"
        
        # 检查内存增长是否超过阈值
        if [ $rss_increase_percent -gt $MAX_MEMORY_INCREASE_PERCENT ]; then
            print_warning "内存增长超过阈值: ${rss_increase_percent}% > ${MAX_MEMORY_INCREASE_PERCENT}%"
        fi
    fi
    
    sleep $INTERVAL
done

# 测试完成
print_info "稳定性测试完成！总运行时间: ${DURATION} 秒"

# 计算最终内存增长
final_rss_increase_percent=$(( (max_rss - initial_rss) * 100 / initial_rss ))
final_vsz_increase_percent=$(( (max_vsz - initial_vsz) * 100 / initial_vsz ))

print_info "内存使用统计:"
print_info "  - 初始RSS: ${initial_rss}KB"
print_info "  - 最大RSS: ${max_rss}KB"
print_info "  - RSS增长: ${final_rss_increase_percent}%"
print_info "  - 初始VSZ: ${initial_vsz}KB"
print_info "  - 最大VSZ: ${max_vsz}KB"
print_info "  - VSZ增长: ${final_vsz_increase_percent}%"

# 检查测试结果
test_passed=true

if [ $final_rss_increase_percent -gt $MAX_MEMORY_INCREASE_PERCENT ]; then
    print_error "测试失败: RSS内存增长超过阈值 (${final_rss_increase_percent}% > ${MAX_MEMORY_INCREASE_PERCENT}%)"
    test_passed=false
fi

if ! ps -p $APP_PID > /dev/null; then
    print_error "测试失败: 程序在测试期间崩溃"
    test_passed=false
else
    print_info "程序运行正常，正在停止..."
    kill $APP_PID
    sleep 2
fi

if $test_passed; then
    print_info "=========================================="
    print_info "✅ 稳定性测试通过！"
    print_info "✅ 连续运行 ${DURATION} 秒无崩溃"
    print_info "✅ 内存增长在允许范围内 (${final_rss_increase_percent}%)"
    print_info "=========================================="
    
    # 生成简要报告
    echo "稳定性测试报告" > stability_report.txt
    echo "================" >> stability_report.txt
    echo "测试时间: $(date)" >> stability_report.txt
    echo "测试时长: ${DURATION} 秒" >> stability_report.txt
    echo "应用程序: $(basename $APP)" >> stability_report.txt
    echo "初始RSS: ${initial_rss} KB" >> stability_report.txt
    echo "最大RSS: ${max_rss} KB" >> stability_report.txt
    echo "RSS增长: ${final_rss_increase_percent}%" >> stability_report.txt
    echo "初始VSZ: ${initial_vsz} KB" >> stability_report.txt
    echo "最大VSZ: ${max_vsz} KB" >> stability_report.txt
    echo "VSZ增长: ${final_vsz_increase_percent}%" >> stability_report.txt
    echo "测试结果: PASS" >> stability_report.txt
    
    print_info "详细报告已保存到: stability_report.txt"
    print_info "内存使用数据已保存到: $CSV_FILE"
    print_info "应用程序日志已保存到: $LOG_FILE"
else
    print_error "=========================================="
    print_error "❌ 稳定性测试失败！"
    print_error "=========================================="
    exit 1
fi

exit 0