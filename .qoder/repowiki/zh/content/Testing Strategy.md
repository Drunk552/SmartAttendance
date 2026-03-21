# 测试策略

<cite>
**本文档引用的文件**
- [README.md](file://README.md)
- [CMakeLists.txt](file://CMakeLists.txt)
- [tools/stability_test.sh](file://tools/stability_test.sh)
- [tools/quick_stability_test.sh](file://tools/quick_stability_test.sh)
- [tools/analyze_stability.py](file://tools/analyze_stability.py)
- [tools/stress_test.sh](file://tools/stress_test.sh)
- [tools/verify_test.sh](file://tools/verify_test.sh)
- [src/main.cpp](file://src/main.cpp)
- [src/business/attendance_rule.h](file://src/business/attendance_rule.h)
- [src/business/event_bus.h](file://src/business/event_bus.h)
- [src/data/db_storage.h](file://src/data/db_storage.h)
- [src/ui/ui_app.h](file://src/ui/ui_app.h)
</cite>

## 目录
1. [引言](#引言)
2. [项目结构](#项目结构)
3. [核心组件](#核心组件)
4. [架构概览](#架构概览)
5. [详细组件分析](#详细组件分析)
6. [依赖分析](#依赖分析)
7. [性能考虑](#性能考虑)
8. [故障排除指南](#故障排除指南)
9. [结论](#结论)

## 引言

SmartAttendance是一个基于嵌入式GUI的智能人脸考勤系统原型，采用LVGL图形框架构建嵌入式界面，集成了人脸识别、考勤规则引擎、数据持久化与报表导出功能。该项目采用了全面的测试策略，包括稳定性测试、压力测试、验证测试等多种测试方法，确保系统在真实硬件环境中的可靠性和稳定性。

## 项目结构

项目采用模块化架构设计，主要分为四个层次：

```mermaid
graph TB
subgraph "应用层"
Main[main.cpp<br/>程序入口]
UI[UI层<br/>界面管理]
Business[业务层<br/>核心逻辑]
Data[数据层<br/>数据库操作]
end
subgraph "测试层"
Stability[稳定性测试<br/>1小时运行]
Quick[快速测试<br/>10分钟验证]
Stress[压力测试<br/>内存监控]
Verify[验证测试<br/>30秒验证]
Analyze[分析工具<br/>CSV数据处理]
end
subgraph "基础设施"
LVGL[LVGL图形库]
OpenCV[OpenCV人脸识别]
SQLite[SQLite数据库]
CMake[CMake构建系统]
end
Main --> UI
Main --> Business
Main --> Data
Stability --> Main
Quick --> Main
Stress --> Main
Verify --> Main
Analyze --> Stability
UI --> LVGL
Business --> OpenCV
Data --> SQLite
Main --> CMake
```

**图表来源**
- [CMakeLists.txt:1-207](file://CMakeLists.txt#L1-L207)
- [src/main.cpp:1-246](file://src/main.cpp#L1-L246)

**章节来源**
- [README.md:42-82](file://README.md#L42-L82)
- [CMakeLists.txt:1-207](file://CMakeLists.txt#L1-L207)

## 核心组件

### 测试框架组件

项目实现了多层次的测试框架，每个组件都有明确的职责和测试目标：

#### 稳定性测试组件
- **1小时稳定性测试**: 监控内存使用、CPU占用、进程状态
- **10分钟快速测试**: 开发阶段的快速验证
- **压力测试**: 持续内存监控和崩溃检测
- **验证测试**: 30秒基本功能验证

#### 分析工具组件
- **CSV数据处理**: 内存使用趋势分析
- **可视化图表生成**: 内存、CPU、线程数趋势图
- **统计报告生成**: 自动化的测试结果报告

**章节来源**
- [tools/stability_test.sh:1-197](file://tools/stability_test.sh#L1-L197)
- [tools/quick_stability_test.sh:1-106](file://tools/quick_stability_test.sh#L1-L106)
- [tools/analyze_stability.py:1-205](file://tools/analyze_stability.py#L1-L205)

## 架构概览

系统采用事件驱动架构，测试策略与主系统架构紧密集成：

```mermaid
sequenceDiagram
participant Test as 测试脚本
participant App as 应用程序
participant Monitor as 监控系统
participant Analyzer as 分析器
Test->>App : 启动应用程序
App->>Monitor : 注册监控回调
Monitor->>Monitor : 定时采样(5秒间隔)
Monitor->>App : 获取进程状态
App-->>Monitor : 返回RSS/VSZ/CPU
Monitor->>Analyzer : 写入CSV文件
Analyzer->>Analyzer : 实时数据分析
Test->>Analyzer : 生成可视化报告
Analyzer-->>Test : 输出测试结果
```

**图表来源**
- [tools/stability_test.sh:85-132](file://tools/stability_test.sh#L85-L132)
- [tools/analyze_stability.py:16-31](file://tools/analyze_stability.py#L16-L31)

## 详细组件分析

### 稳定性测试系统

稳定性测试系统是整个测试策略的核心，实现了完整的自动化监控流程：

#### 测试执行流程

```mermaid
flowchart TD
Start([开始测试]) --> Init[初始化测试环境]
Init --> Launch[启动应用程序]
Launch --> Wait[等待初始化完成]
Wait --> CheckProcess{进程存活?}
CheckProcess --> |否| Fail[测试失败]
CheckProcess --> |是| Monitor[开始监控]
Monitor --> Sample[采样数据<br/>RSS/VSZ/CPU/线程]
Sample --> WriteCSV[写入CSV文件]
WriteCSV --> CheckThreshold{超过阈值?}
CheckThreshold --> |是| Warn[发出警告]
CheckThreshold --> |否| Continue[继续监控]
Warn --> Continue
Continue --> Duration{测试时长?}
Duration --> |未完成| Sample
Duration --> |完成| Finalize[结束测试]
Finalize --> GenerateReport[生成报告]
GenerateReport --> Pass[测试通过]
Fail --> End([结束])
Pass --> End
```

**图表来源**
- [tools/stability_test.sh:85-132](file://tools/stability_test.sh#L85-L132)
- [tools/stability_test.sh:149-164](file://tools/stability_test.sh#L149-L164)

#### 内存监控机制

稳定性测试实现了精确的内存使用监控：

| 监控指标 | 阈值 | 描述 |
|---------|------|------|
| RSS内存增长 | ≤20% | 物理内存使用增长率 |
| VSZ内存增长 | ≤20% | 虚拟内存使用增长率 |
| CPU使用率 | ≤80% | 处理器使用率阈值 |
| 测试时长 | ≥3500秒 | 1小时测试的最低要求 |

**章节来源**
- [tools/stability_test.sh:10-16](file://tools/stability_test.sh#L10-L16)
- [tools/analyze_stability.py:83-99](file://tools/analyze_stability.py#L83-L99)

### 快速验证测试

快速验证测试专为开发阶段设计，提供即时反馈：

#### 验证流程

```mermaid
flowchart LR
Start([开始验证]) --> CheckApp[检查应用程序]
CheckApp --> Launch[启动应用]
Launch --> InitWait[等待初始化]
InitWait --> CheckAlive{进程存活?}
CheckAlive --> |否| LogError[记录错误日志]
CheckAlive --> |是| RunTest[运行30秒测试]
RunTest --> Monitor[监控进程状态]
Monitor --> CheckCrash{检测崩溃?}
CheckCrash --> |是| ReportFail[报告失败]
CheckCrash --> |否| CalcMetrics[计算内存指标]
CalcMetrics --> StopApp[停止应用程序]
StopApp --> ReportPass[报告通过]
LogError --> End([结束])
ReportFail --> End
ReportPass --> End
```

**图表来源**
- [tools/verify_test.sh:74-101](file://tools/verify_test.sh#L74-L101)
- [tools/verify_test.sh:134-153](file://tools/verify_test.sh#L134-L153)

**章节来源**
- [tools/verify_test.sh:1-162](file://tools/verify_test.sh#L1-L162)

### 压力测试组件

压力测试专注于长期运行稳定性：

#### 压力测试执行

```mermaid
sequenceDiagram
participant Script as 压力测试脚本
participant App as 应用程序
participant System as 系统监控
Script->>App : 启动应用程序
loop 每5秒
Script->>System : 检查进程状态
System->>Script : 返回进程信息
Script->>System : 获取RSS内存
System->>Script : 返回内存使用
alt 进程崩溃
Script->>Script : 检测到崩溃
Script->>Script : 退出并返回错误码
end
end
Script->>App : 结束测试并清理进程
```

**图表来源**
- [tools/stress_test.sh:8-17](file://tools/stress_test.sh#L8-L17)

**章节来源**
- [tools/stress_test.sh:1-20](file://tools/stress_test.sh#L1-L20)

### 分析工具系统

分析工具提供了完整的数据处理和可视化能力：

#### 数据分析流程

```mermaid
flowchart TD
CSVFile[CSV数据文件] --> LoadData[加载CSV数据]
LoadData --> BasicStats[基本统计分析]
BasicStats --> MemoryTrend[内存使用趋势]
BasicStats --> CPUPerformance[CPU性能分析]
BasicStats --> ThreadAnalysis[线程变化分析]
MemoryTrend --> MemoryChart[内存趋势图]
CPUPerformance --> CPUSeries[CPU使用率图]
ThreadAnalysis --> ThreadChart[线程数变化图]
MemoryChart --> Histogram[内存分布直方图]
CPUSeries --> Report[生成报告]
Histogram --> Report
ThreadChart --> Report
Report --> Output[输出分析结果]
```

**图表来源**
- [tools/analyze_stability.py:33-112](file://tools/analyze_stability.py#L33-L112)
- [tools/analyze_stability.py:114-171](file://tools/analyze_stability.py#L114-L171)

**章节来源**
- [tools/analyze_stability.py:1-205](file://tools/analyze_stability.py#L1-L205)

## 依赖分析

测试系统的依赖关系清晰明确，各组件协同工作：

```mermaid
graph TB
subgraph "测试执行层"
StabilityTest[稳定性测试]
QuickTest[快速测试]
StressTest[压力测试]
VerifyTest[验证测试]
end
subgraph "数据处理层"
CSVProcessor[CSV处理器]
DataAnalyzer[数据分析器]
ChartGenerator[图表生成器]
end
subgraph "系统监控层"
ProcessMonitor[进程监控器]
MemorySampler[内存采样器]
CPUMonitor[CPU监控器]
end
subgraph "输出层"
TextReport[文本报告]
VisualCharts[可视化图表]
Summary[摘要报告]
end
StabilityTest --> ProcessMonitor
QuickTest --> ProcessMonitor
StressTest --> ProcessMonitor
VerifyTest --> ProcessMonitor
ProcessMonitor --> CSVProcessor
CSVProcessor --> DataAnalyzer
DataAnalyzer --> ChartGenerator
ChartGenerator --> VisualCharts
DataAnalyzer --> TextReport
ProcessMonitor --> Summary
```

**图表来源**
- [CMakeLists.txt:158-201](file://CMakeLists.txt#L158-L201)
- [tools/analyze_stability.py:16-31](file://tools/analyze_stability.py#L16-L31)

**章节来源**
- [CMakeLists.txt:158-201](file://CMakeLists.txt#L158-L201)

## 性能考虑

测试策略充分考虑了性能影响和资源消耗：

### 内存使用优化
- **采样间隔**: 5秒间隔平衡精度与性能
- **数据缓冲**: CSV文件轮询写入，避免内存溢出
- **进程监控**: 轻量级ps命令调用，减少系统开销

### CPU效率
- **非阻塞监控**: 使用sleep进行时间控制
- **条件检查**: 只在必要时进行数据采样
- **阈值优化**: 合理的内存增长阈值避免误报

### 存储管理
- **临时文件**: 自动清理测试产生的中间文件
- **CSV格式**: 结构化数据便于后续分析
- **报告生成**: 自动生成多种格式的测试报告

## 故障排除指南

### 常见问题诊断

#### 测试失败排查
1. **应用程序启动失败**
   - 检查构建是否成功
   - 验证依赖库是否正确安装
   - 查看应用程序日志文件

2. **内存泄漏检测**
   - 分析内存增长趋势图
   - 检查RSS vs VSZ差异
   - 审查长时间运行的内存分配

3. **进程崩溃处理**
   - 检查崩溃时的系统状态
   - 分析最近的系统调用
   - 验证资源清理完整性

#### 性能问题解决
- **CPU使用率过高**: 检查监控频率设置
- **内存增长异常**: 审查内存分配模式
- **测试时间不足**: 调整测试时长配置

**章节来源**
- [tools/stability_test.sh:150-164](file://tools/stability_test.sh#L150-L164)
- [tools/verify_test.sh:83-89](file://tools/verify_test.sh#L83-L89)

## 结论

SmartAttendance项目的测试策略体现了全面性和实用性的特点：

### 测试策略优势
1. **多层次覆盖**: 从快速验证到长期稳定性测试的完整体系
2. **自动化程度高**: CMake集成的测试目标，简化测试流程
3. **数据驱动分析**: CSV数据和可视化图表提供客观评估
4. **实时监控**: 进程状态和资源使用的持续跟踪

### 技术创新点
- **集成式测试框架**: 将测试工具与构建系统深度集成
- **多维度监控**: 同时监控内存、CPU、线程等多个指标
- **智能化分析**: Python脚本自动分析测试结果并生成报告
- **灵活的阈值配置**: 可根据硬件环境调整测试参数

### 改进建议
1. **增加单元测试**: 为关键业务逻辑添加单元测试
2. **扩展测试场景**: 增加更多边界条件和异常场景测试
3. **性能基准测试**: 建立性能基线用于回归测试
4. **持续集成**: 集成到CI/CD流程中实现自动化测试

该测试策略为嵌入式GUI应用提供了可靠的质量保障，确保系统在真实硬件环境中的稳定运行。