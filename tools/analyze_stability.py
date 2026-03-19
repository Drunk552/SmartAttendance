#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
智能考勤系统稳定性测试分析工具
用于分析内存使用数据并生成可视化报告
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import os
from datetime import datetime

def analyze_csv(csv_file):
    """分析CSV文件并生成报告"""
    if not os.path.exists(csv_file):
        print(f"错误: CSV文件不存在: {csv_file}")
        return None
    
    try:
        df = pd.read_csv(csv_file)
        print(f"成功读取数据: {len(df)} 条记录")
        print(f"时间范围: {df['timestamp'].iloc[0]} 到 {df['timestamp'].iloc[-1]}")
        print(f"运行时长: {df['elapsed_seconds'].iloc[-1]} 秒")
        
        return df
    except Exception as e:
        print(f"读取CSV文件失败: {e}")
        return None

def generate_report(df, output_dir="."):
    """生成详细报告和图表"""
    if df is None or len(df) == 0:
        print("错误: 没有数据可分析")
        return
    
    os.makedirs(output_dir, exist_ok=True)
    
    # 基本统计
    initial_rss = df['rss_kb'].iloc[0]
    max_rss = df['rss_kb'].max()
    min_rss = df['rss_kb'].min()
    avg_rss = df['rss_kb'].mean()
    
    initial_vsz = df['vsz_kb'].iloc[0]
    max_vsz = df['vsz_kb'].max()
    
    rss_increase_percent = ((max_rss - initial_rss) / initial_rss) * 100
    vsz_increase_percent = ((max_vsz - initial_vsz) / initial_vsz) * 100
    
    avg_cpu = df['cpu_percent'].mean()
    max_cpu = df['cpu_percent'].max()
    
    avg_threads = df['threads'].mean()
    
    # 生成文本报告
    report_file = os.path.join(output_dir, "stability_analysis_report.txt")
    with open(report_file, 'w', encoding='utf-8') as f:
        f.write("智能考勤系统稳定性测试分析报告\n")
        f.write("=" * 50 + "\n")
        f.write(f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"数据文件: {os.path.basename(csv_file) if 'csv_file' in locals() else 'N/A'}\n")
        f.write(f"数据记录数: {len(df)}\n")
        f.write(f"测试时长: {df['elapsed_seconds'].iloc[-1]} 秒\n")
        f.write("\n内存使用统计:\n")
        f.write(f"  初始RSS: {initial_rss:,.0f} KB\n")
        f.write(f"  最大RSS: {max_rss:,.0f} KB\n")
        f.write(f"  最小RSS: {min_rss:,.0f} KB\n")
        f.write(f"  平均RSS: {avg_rss:,.0f} KB\n")
        f.write(f"  RSS增长: {rss_increase_percent:.2f}%\n")
        f.write(f"  初始VSZ: {initial_vsz:,.0f} KB\n")
        f.write(f"  最大VSZ: {max_vsz:,.0f} KB\n")
        f.write(f"  VSZ增长: {vsz_increase_percent:.2f}%\n")
        f.write("\nCPU使用统计:\n")
        f.write(f"  平均CPU: {avg_cpu:.2f}%\n")
        f.write(f"  最大CPU: {max_cpu:.2f}%\n")
        f.write("\n线程统计:\n")
        f.write(f"  平均线程数: {avg_threads:.2f}\n")
        f.write("\n稳定性评估:\n")
        
        # 评估标准
        if rss_increase_percent <= 20:
            f.write("  ✅ RSS内存增长在允许范围内 (<20%)\n")
        else:
            f.write(f"  ⚠️  RSS内存增长超过阈值: {rss_increase_percent:.2f}% > 20%\n")
            
        if avg_cpu <= 80:
            f.write("  ✅ CPU使用率正常 (<80%)\n")
        else:
            f.write(f"  ⚠️  CPU使用率偏高: {avg_cpu:.2f}% > 80%\n")
            
        # 检查是否有崩溃（数据是否完整）
        expected_duration = df['elapsed_seconds'].iloc[-1]
        if expected_duration >= 3500:  # 接近1小时
            f.write("  ✅ 测试时长达到要求 (>3500秒)\n")
        else:
            f.write(f"  ⚠️  测试时长不足: {expected_duration}秒 < 3600秒\n")
    
    print(f"文本报告已生成: {report_file}")
    
    # 生成图表
    generate_charts(df, output_dir)
    
    return {
        'initial_rss': initial_rss,
        'max_rss': max_rss,
        'rss_increase_percent': rss_increase_percent,
        'avg_cpu': avg_cpu,
        'test_duration': df['elapsed_seconds'].iloc[-1]
    }

def generate_charts(df, output_dir):
    """生成可视化图表"""
    plt.figure(figsize=(15, 10))
    
    # 1. 内存使用趋势图
    plt.subplot(2, 2, 1)
    plt.plot(df['elapsed_seconds'] / 60, df['rss_kb'] / 1024, 'b-', label='RSS (MB)', linewidth=2)
    plt.plot(df['elapsed_seconds'] / 60, df['vsz_kb'] / 1024, 'r--', label='VSZ (MB)', linewidth=1, alpha=0.7)
    plt.xlabel('运行时间 (分钟)')
    plt.ylabel('内存使用 (MB)')
    plt.title('内存使用趋势')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # 2. CPU使用率
    plt.subplot(2, 2, 2)
    plt.plot(df['elapsed_seconds'] / 60, df['cpu_percent'], 'g-', linewidth=2)
    plt.xlabel('运行时间 (分钟)')
    plt.ylabel('CPU使用率 (%)')
    plt.title('CPU使用率趋势')
    plt.ylim(0, max(100, df['cpu_percent'].max() * 1.1))
    plt.grid(True, alpha=0.3)
    
    # 3. 内存增长百分比
    plt.subplot(2, 2, 3)
    initial_rss = df['rss_kb'].iloc[0]
    rss_increase = ((df['rss_kb'] - initial_rss) / initial_rss) * 100
    plt.plot(df['elapsed_seconds'] / 60, rss_increase, 'purple', linewidth=2)
    plt.axhline(y=20, color='r', linestyle='--', alpha=0.5, label='阈值 (20%)')
    plt.xlabel('运行时间 (分钟)')
    plt.ylabel('RSS增长百分比 (%)')
    plt.title('内存增长百分比')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # 4. 线程数变化
    plt.subplot(2, 2, 4)
    plt.plot(df['elapsed_seconds'] / 60, df['threads'], 'orange', linewidth=2)
    plt.xlabel('运行时间 (分钟)')
    plt.ylabel('线程数')
    plt.title('线程数变化')
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    chart_file = os.path.join(output_dir, "stability_charts.png")
    plt.savefig(chart_file, dpi=150)
    print(f"图表已生成: {chart_file}")
    
    # 生成内存分布直方图
    plt.figure(figsize=(10, 6))
    plt.hist(df['rss_kb'] / 1024, bins=30, alpha=0.7, color='blue', edgecolor='black')
    plt.xlabel('RSS内存使用 (MB)')
    plt.ylabel('频率')
    plt.title('内存使用分布直方图')
    plt.grid(True, alpha=0.3)
    hist_file = os.path.join(output_dir, "memory_histogram.png")
    plt.savefig(hist_file, dpi=150)
    print(f"直方图已生成: {hist_file}")

def main():
    """主函数"""
    if len(sys.argv) < 2:
        print("用法: python analyze_stability.py <csv文件> [输出目录]")
        print("示例: python analyze_stability.py memory_usage.csv ./reports")
        return
    
    csv_file = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "."
    
    print("智能考勤系统稳定性测试分析工具")
    print("=" * 50)
    
    # 分析数据
    df = analyze_csv(csv_file)
    if df is not None:
        # 生成报告
        results = generate_report(df, output_dir)
        
        if results:
            print("\n分析完成!")
            print(f"测试时长: {results['test_duration']} 秒")
            print(f"内存增长: {results['rss_increase_percent']:.2f}%")
            print(f"平均CPU: {results['avg_cpu']:.2f}%")
            
            # 简单评估
            if results['rss_increase_percent'] <= 20 and results['test_duration'] >= 3500:
                print("\n✅ 稳定性测试通过!")
            else:
                print("\n⚠️  稳定性测试需要进一步检查")

if __name__ == "__main__":
    main()