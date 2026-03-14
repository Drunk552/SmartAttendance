# 用户管理DAO

<cite>
**本文档引用的文件**
- [db_storage.h](file://src/data/db_storage.h)
- [db_storage.cpp](file://src/data/db_storage.cpp)
- [auth_service.h](file://src/business/auth_service.h)
- [auth_service.cpp](file://src/business/auth_service.cpp)
- [face_demo.h](file://src/business/face_demo.h)
- [face_demo.cpp](file://src/business/face_demo.cpp)
- [ui_controller.cpp](file://src/ui/ui_controller.cpp)
- [main.cpp](file://src/main.cpp)
</cite>

## 目录
1. [简介](#简介)
2. [项目结构](#项目结构)
3. [核心组件](#核心组件)
4. [架构概览](#架构概览)
5. [详细组件分析](#详细组件分析)
6. [依赖关系分析](#依赖关系分析)
7. [性能考虑](#性能考虑)
8. [故障排除指南](#故障排除指南)
9. [结论](#结论)

## 简介

用户管理DAO模块是SmartAttendance智能考勤系统的核心数据访问层，负责用户信息的完整生命周期管理。该模块实现了基于SQLite的高性能数据持久化，支持用户注册、批量导入、删除、查询等核心操作，同时集成了密码哈希处理、人脸特征存储、指纹特征管理、权限控制等关键业务逻辑。

该模块采用分层架构设计，通过数据层抽象隐藏数据库细节，为上层业务逻辑提供统一的接口。模块支持多线程安全操作，使用共享-排他锁机制确保数据一致性，同时优化了大数据量场景下的性能表现。

## 项目结构

SmartAttendance项目采用清晰的分层架构，用户管理DAO模块位于数据层，主要文件组织如下：

```mermaid
graph TB
subgraph "应用层"
UI[用户界面层]
Business[业务逻辑层]
end
subgraph "数据访问层"
DAO[用户管理DAO]
DB[数据库引擎]
end
subgraph "基础设施"
SQLite[SQLite数据库]
OpenCV[图像处理库]
end
UI --> Business
Business --> DAO
DAO --> DB
DB --> SQLite
DAO --> OpenCV
```

**图表来源**
- [db_storage.h:1-596](file://src/data/db_storage.h#L1-L596)
- [db_storage.cpp:1-2171](file://src/data/db_storage.cpp#L1-L2171)

**章节来源**
- [db_storage.h:1-596](file://src/data/db_storage.h#L1-L596)
- [db_storage.cpp:1-2171](file://src/data/db_storage.cpp#L1-L2171)

## 核心组件

### 数据结构设计

用户管理DAO模块的核心数据结构围绕UserData展开，该结构体设计体现了完整的用户信息管理需求：

```mermaid
classDiagram
class UserData {
+int id
+string name
+string password
+string card_id
+int role
+int dept_id
+int default_shift_id
+string dept_name
+vector~uchar~ face_feature
+string avatar_path
+vector~uint8_t~ fingerprint_feature
+string position
}
class DeptInfo {
+int id
+string name
}
class ShiftInfo {
+int id
+string name
+string s1_start
+string s1_end
+string s2_start
+string s2_end
+string s3_start
+string s3_end
+int cross_day
}
class RuleConfig {
+string company_name
+int late_threshold
+int early_leave_threshold
+int device_id
+int volume
+int screensaver_time
+int max_admins
+int relay_delay
+int wiegand_fmt
+int duplicate_punch_limit
+string language
+string date_format
+int return_home_delay
+int warning_record_count
+int sat_work
+int sun_work
}
UserData --> DeptInfo : "关联"
UserData --> ShiftInfo : "关联"
RuleConfig --> UserData : "配置"
```

**图表来源**
- [db_storage.h:104-142](file://src/data/db_storage.h#L104-L142)
- [db_storage.h:22-28](file://src/data/db_storage.h#L22-L28)
- [db_storage.h:34-55](file://src/data/db_storage.h#L34-L55)
- [db_storage.h:61-86](file://src/data/db_storage.h#L61-L86)

### 核心接口设计

用户管理DAO提供了完整的CRUD操作接口，涵盖用户生命周期的各个阶段：

| 操作类型 | 接口名称 | 功能描述 | 性能特点 |
|---------|----------|----------|----------|
| 创建 | `db_add_user` | 用户注册，支持人脸特征存储 | 事务处理，BLOB压缩 |
| 批量 | `db_batch_add_users` | 批量导入员工数据 | SQLite事务加速 |
| 查询 | `db_get_user_info` | 获取用户详细信息 | 懒加载BLOB数据 |
| 更新 | `db_update_user_basic` | 更新用户基本信息 | 条件更新，外键约束 |
| 删除 | `db_delete_user` | 删除用户记录 | 级联删除考勤记录 |

**章节来源**
- [db_storage.h:317-420](file://src/data/db_storage.h#L317-L420)
- [db_storage.cpp:748-803](file://src/data/db_storage.cpp#L748-L803)
- [db_storage.cpp:805-904](file://src/data/db_storage.cpp#L805-L904)
- [db_storage.cpp:906-977](file://src/data/db_storage.cpp#L906-L977)

## 架构概览

用户管理DAO模块采用分层架构设计，通过接口抽象实现数据访问的统一入口：

```mermaid
sequenceDiagram
participant Client as "客户端"
participant DAO as "用户管理DAO"
participant DB as "SQLite数据库"
participant FS as "文件系统"
Client->>DAO : db_add_user(user, face_img)
DAO->>FS : 保存人脸图片到磁盘
FS-->>DAO : 返回文件路径
DAO->>DB : 插入用户记录
DB-->>DAO : 返回用户ID
DAO-->>Client : 返回新用户ID
Note over Client,DB : 用户注册完整流程
```

**图表来源**
- [db_storage.cpp:748-803](file://src/data/db_storage.cpp#L748-L803)

### 数据库设计

用户管理DAO模块的数据库设计遵循关系型数据库最佳实践，通过外键约束确保数据完整性：

```mermaid
erDiagram
USERS {
integer id PK
string name
string password
string card_id
integer privilege
blob face_data
string avatar_path
blob fingerprint_data
integer dept_id FK
integer default_shift_id FK
}
DEPARTMENTS {
integer id PK
string name UK
}
SHIFTS {
integer id PK
string name
string s1_start
string s1_end
string s2_start
string s2_end
string s3_start
string s3_end
integer cross_day
}
ATTENDANCE {
integer id PK
integer user_id FK
integer shift_id FK
string image_path
integer timestamp
integer status
}
USERS ||--|| DEPARTMENTS : "dept_id"
USERS ||--|| SHIFTS : "default_shift_id"
ATTENDANCE ||--|| USERS : "user_id"
ATTENDANCE ||--|| SHIFTS : "shift_id"
```

**图表来源**
- [db_storage.cpp:181-256](file://src/data/db_storage.cpp#L181-L256)

**章节来源**
- [db_storage.cpp:181-256](file://src/data/db_storage.cpp#L181-L256)

## 详细组件分析

### 用户注册流程

用户注册是用户管理DAO的核心业务流程，涉及多步骤的原子性操作：

```mermaid
flowchart TD
Start([开始注册]) --> Validate["验证用户输入"]
Validate --> InputValid{"输入有效?"}
InputValid --> |否| ReturnError["返回错误"]
InputValid --> |是| SaveAvatar["保存人脸图片到磁盘"]
SaveAvatar --> SaveAvatarSuccess{"保存成功?"}
SaveAvatarSuccess --> |否| Cleanup["清理临时文件"]
Cleanup --> ReturnError
SaveAvatarSuccess --> |是| InsertUser["插入用户记录到数据库"]
InsertUser --> InsertSuccess{"插入成功?"}
InsertSuccess --> |否| Rollback["回滚事务"]
Rollback --> ReturnError
InsertSuccess --> |是| ReturnID["返回用户ID"]
ReturnID --> End([注册完成])
```

**图表来源**
- [db_storage.cpp:748-803](file://src/data/db_storage.cpp#L748-L803)

#### 密码哈希处理

系统采用简单哈希算法处理用户密码，确保密码存储的安全性：

| 哈希算法 | 特点 | 安全性评估 |
|---------|------|-----------|
| std::hash | 简单快速 | 低安全性，仅用于演示 |
| SHA-256 | 安全可靠 | 高安全性，推荐使用 |
| bcrypt | 专业加密 | 最佳安全性，推荐生产环境 |

**章节来源**
- [db_storage.cpp:304-314](file://src/data/db_storage.cpp#L304-L314)
- [auth_service.cpp:9-37](file://src/business/auth_service.cpp#L9-L37)

### 人脸特征管理

人脸特征存储采用OpenCV图像编码技术，优化存储空间和传输效率：

```mermaid
classDiagram
class FaceFeatureStorage {
+matToBytes(image) vector~uchar~
+bytesToMat(bytes) Mat
+saveFaceImage(user_id, face_img) bool
+loadFaceImage(user_id) Mat
-compressImage(image) vector~uchar~
-extractFeatures(image) vector~uchar~
}
class UserData {
+vector~uchar~ face_feature
+string avatar_path
}
FaceFeatureStorage --> UserData : "管理"
```

**图表来源**
- [db_storage.cpp:69-89](file://src/data/db_storage.cpp#L69-L89)

#### 指纹特征处理

指纹特征管理支持二进制数据的完整生命周期管理：

| 操作类型 | 接口名称 | 功能描述 | 数据处理 |
|---------|----------|----------|----------|
| 存储 | `db_update_user_fingerprint` | 更新用户指纹特征 | BLOB二进制存储 |
| 查询 | `db_get_user_info` | 获取用户指纹数据 | 懒加载优化 |
| 清理 | `db_update_user_face` | 更新人脸头像 | 文件系统管理 |

**章节来源**
- [db_storage.cpp:1219-1262](file://src/data/db_storage.cpp#L1219-L1262)
- [db_storage.cpp:1128-1192](file://src/data/db_storage.cpp#L1128-L1192)

### 权限管理系统

系统采用基于角色的权限控制模型，支持管理员和普通用户的权限分离：

```mermaid
stateDiagram-v2
[*] --> Guest
Guest --> Employee : 注册用户
Employee --> Admin : 管理员授权
Admin --> Employee : 权限撤销
state Admin {
[*] --> FullAccess
FullAccess --> UserManagement : 管理用户
FullAccess --> SystemConfig : 配置系统
FullAccess --> ReportGeneration : 生成报表
}
state Employee {
[*] --> LimitedAccess
LimitedAccess --> ClockIn : 打卡考勤
LimitedAccess --> ViewSchedule : 查看排班
LimitedAccess --> ViewReports : 查看报表
}
```

**图表来源**
- [db_storage.h:117-119](file://src/data/db_storage.h#L117-L119)

#### 部门关联管理

用户与部门的关联通过外键约束实现，确保数据一致性：

| 操作类型 | 接口名称 | 功能描述 | 约束条件 |
|---------|----------|----------|----------|
| 关联 | `db_update_user_basic` | 更新用户部门 | 外键约束SET NULL |
| 查询 | `db_get_user_info` | 获取用户部门信息 | LEFT JOIN联表查询 |
| 解除 | `db_delete_department` | 删除部门 | 级联更新用户dept_id |

**章节来源**
- [db_storage.cpp:1097-1125](file://src/data/db_storage.cpp#L1097-L1125)
- [db_storage.cpp:448-461](file://src/data/db_storage.cpp#L448-L461)

### 批量数据导入

批量导入功能支持U盘/网络同步场景，采用SQLite事务优化性能：

```mermaid
sequenceDiagram
participant Client as "客户端"
participant DAO as "用户管理DAO"
participant DB as "SQLite数据库"
Client->>DAO : db_batch_add_users(users_list)
DAO->>DB : BEGIN TRANSACTION
loop 遍历用户列表
DAO->>DB : INSERT OR REPLACE INTO users
DB-->>DAO : 返回执行结果
end
alt 所有用户导入成功
DAO->>DB : COMMIT
DB-->>DAO : 提交成功
else 发生错误
DAO->>DB : ROLLBACK
DB-->>DAO : 回滚事务
end
DAO-->>Client : 返回批量导入结果
```

**图表来源**
- [db_storage.cpp:805-904](file://src/data/db_storage.cpp#L805-L904)

**章节来源**
- [db_storage.cpp:805-904](file://src/data/db_storage.cpp#L805-L904)

## 依赖关系分析

用户管理DAO模块的依赖关系体现了清晰的分层架构：

```mermaid
graph TB
subgraph "外部依赖"
OpenCV[OpenCV图像库]
SQLite[SQLite数据库引擎]
Boost[Boost文件系统]
end
subgraph "内部模块"
DAO[用户管理DAO]
Auth[认证服务]
Business[业务逻辑]
UI[用户界面]
end
DAO --> OpenCV
DAO --> SQLite
DAO --> Boost
Auth --> DAO
Business --> DAO
UI --> Business
Business --> Auth
```

**图表来源**
- [db_storage.h:10-14](file://src/data/db_storage.h#L10-L14)
- [auth_service.h:1-46](file://src/business/auth_service.h#L1-L46)

### 线程安全设计

模块采用共享-排他锁机制确保多线程环境下的数据一致性：

| 锁类型 | 使用场景 | 作用范围 |
|--------|----------|----------|
| shared_mutex | 读操作 | 允许多个读操作并发 |
| unique_lock | 写操作 | 独占数据库连接 |
| RAII封装 | 语句管理 | 自动资源清理 |

**章节来源**
- [db_storage.cpp:35-65](file://src/data/db_storage.cpp#L35-L65)

## 性能考虑

### 数据库优化策略

用户管理DAO模块采用了多项性能优化措施：

1. **WAL模式优化**：启用Write-Ahead Logging模式提升并发性能
2. **预编译语句**：缓存高频SQL语句减少解析开销
3. **联合索引**：为考勤查询建立复合索引
4. **懒加载策略**：BLOB数据按需加载避免内存浪费

### 缓存机制

系统实现了多层次的缓存策略：

| 缓存层级 | 缓存内容 | 命中策略 | 清理机制 |
|----------|----------|----------|----------|
| L1缓存 | 用户ID映射 | 启动时加载 | 程序退出清理 |
| L2缓存 | 用户基本信息 | 按需加载 | 内存不足时淘汰 |
| L3缓存 | 头像文件 | 文件系统缓存 | 磁盘空间监控 |

**章节来源**
- [db_storage.cpp:275-282](file://src/data/db_storage.cpp#L275-L282)
- [face_demo.cpp:603-667](file://src/business/face_demo.cpp#L603-L667)

## 故障排除指南

### 常见问题诊断

| 问题类型 | 症状描述 | 可能原因 | 解决方案 |
|----------|----------|----------|----------|
| 数据库连接失败 | `Can't open DB`错误 | 权限不足或文件损坏 | 检查文件权限和完整性 |
| 用户注册失败 | 返回-1 | 人脸图片保存失败 | 检查磁盘空间和路径权限 |
| 密码验证失败 | 认证结果为WRONG_PASSWORD | 密码哈希不匹配 | 检查哈希算法一致性 |
| 指纹识别错误 | 认证结果为WRONG_FINGERPRINT | 指纹模板不匹配 | 重新录入指纹特征 |

### 性能监控指标

系统提供了完善的性能监控能力：

```mermaid
flowchart LR
subgraph "监控指标"
A[数据库连接数]
B[查询响应时间]
C[内存使用率]
D[磁盘IO吞吐]
end
subgraph "告警机制"
E[连接数告警]
F[响应时间告警]
G[内存使用告警]
H[IO延迟告警]
end
A --> E
B --> F
C --> G
D --> H
```

**图表来源**
- [db_storage.cpp:1771-1799](file://src/data/db_storage.cpp#L1771-L1799)

**章节来源**
- [db_storage.cpp:1771-1799](file://src/data/db_storage.cpp#L1771-L1799)

## 结论

用户管理DAO模块通过精心设计的数据结构、完善的接口抽象和高效的实现策略，为SmartAttendance系统提供了稳定可靠的用户管理能力。模块不仅满足了基本的CRUD操作需求，还集成了密码哈希、人脸特征存储、指纹管理、权限控制等高级功能。

模块的设计充分考虑了性能优化和线程安全，在保证数据一致性的同时提升了系统整体性能。通过分层架构和接口抽象，模块为上层业务逻辑提供了清晰、易用的访问接口，降低了系统的耦合度。

未来可以在以下方面进一步优化：
1. 引入更安全的密码哈希算法（如bcrypt）
2. 实现分布式缓存机制提升大规模部署性能
3. 增加数据备份和恢复机制
4. 扩展更多生物特征识别支持