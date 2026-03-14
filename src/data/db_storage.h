/**
 * @file db_storage.h
 * @brief 数据层核心接口 
 * @details 封装数据库操作，提供部门、班次、用户及考勤记录的增删改查（DAO）接口。
 */

#ifndef DB_STORAGE_H
#define DB_STORAGE_H

#include <opencv2/core.hpp>
#include <string> // 需要处理字符串
#include <vector>
#include <utility> // for std::pair
#include <optional>

// ================= 数据结构定义(Data Structures) =================

/**
 * @brief 部门信息结构体
 * @details 对应数据库 `departments` 表
 */
struct DeptInfo {
    /// @brief 部门ID (数据库自增主键)
    int id;

    /// @brief 部门名称 (e.g. "研发部")
    std::string name;
};

/**
 * @brief 班次信息结构体（支持业务文档的3个时段）
 * @details 对应数据库 `shifts` 表，完全匹配业务文档中的班次设置格式
 *
 * 时间字段处理规则：
 * - 空字符串或"--:--"：表示该时段无考勤要求
 * - "HH:MM"格式：有效考勤时间
 * - 业务层负责具体的时间验证和逻辑处理
 */
struct ShiftInfo {
    /// @brief 班次ID (数据库自增主键，1-10对应业务文档中的班次号)
    int id;
    
    /// @brief 班次名称 (e.g. "早班")
    std::string name;

    /// @brief 时段一：主要工作时段（对应业务文档"时段一"）
    std::string s1_start;       // 上班时间，如"08:00"
    std::string s1_end;         // 下班时间，如"12:00"

    /// @brief 时段二：下午工作时段（对应业务文档"时段二"）
    std::string s2_start;       // 上班时间，如"14:00"
    std::string s2_end;         // 下班时间，如"18:00"

    /// @brief 时段三：加班时段（对应业务文档"加班"时段）
    std::string s3_start;       // 加班上班时间，如"19:00"
    std::string s3_end;         // 加班下班时间，如"21:00"
    
    /// @brief 是否跨天 (0: 当天, 1: 次日)
    int cross_day;
};

/**
 * @brief 部门排班条目
 * @details 用于单个部门单天的排班记录
 * 对应业务文档规则："填写的数字代表班次号(1-10)，留空或0代表节假日"
 */
struct DeptScheduleEntry {
    int dept_id;                // 部门ID
    int day_of_week;            // 星期几：0=周日，1=周一，...，6=周六
    int shift_id;               // 班次ID，0表示节假日
};

/**
 * @brief 部门完整排班视图
 * @details 用于UI显示，包含部门信息和一周排班
 */
struct DeptScheduleView {
    int dept_id;
    std::string dept_name;
    int shifts[7];              // 7天的班次安排，索引对应星期几
};

/**
 * @brief 系统考勤规则配置
 * @details 对应数据库 `attendance_rules` 表
 */
struct RuleConfig {
    std::string company_name;
    int late_threshold;       // 允许迟到分钟数 (默认 15)
    int early_leave_threshold;// 允许早退分钟数 (默认 0)
    int device_id;            // 机器号 (1-255)
    int volume;               // 音量 (0-100)
    int screensaver_time;     // 屏保等待时间(分)
    int max_admins;           // 管理员人数限制
    // --- 门禁参数  ---
    int relay_delay;          // 继电器延时(秒)
    int wiegand_fmt;          // 韦根格式 (26/34)

    int duplicate_punch_limit; // 防重复打卡时间(分钟) 
    std::string language;      // 语言设置 (如 "zh-CN", "en-US")
    std::string date_format;   // 日期格式 (如 "YYYY-MM-DD")
    int return_home_delay;     // 返回主界面超时时间(秒)
    int warning_record_count;  // 记录警告数阈值

    // 【流程图节点K】周六/周日是否上班的规则开关
    // 对应流程图中：无论通过个人、部门还是默认班次路径，
    // 进入考勤计算前都必须经过此节点判断
    // 0 = 不上班（该天视为休息，返回无排班）
    // 1 = 上班（正常走考勤计算）
    int sat_work; // 星期六是否上班 (默认 0: 不上班)
    int sun_work; // 星期日是否上班 (默认 0: 不上班)
};

/**
 * @brief 定时响铃配置结构体 
 */
struct BellSchedule {
    int id;             // 序号 (1-16)
    std::string time;   // 响铃时间 "HH:MM"
    int duration;       // 响铃时长 (秒)
    int days_mask;      // 周期掩码 (位操作): bit0=周日, bit1=周一 ... bit6=周六
                        // 例如: 01111110 (0x7E) 代表周一到周五
    bool enabled;       // 是否启用
};

/**
 * @brief 用户完整信息结构体
 * @details 对应数据库 `users` 表，包含身份、权限及生物特征
 */
struct UserData {
    /// @brief 工号/用户ID (数据库自增主键)
    int id;                 

    /// @brief 姓名 (支持中英文)
    std::string name;       

    /// @brief 登录密码 (用于输入验证)
    std::string password;   

    /// @brief IC/ID卡号 (用于刷卡验证)
    std::string card_id;    

    /// @brief  权限等级
    /// @note 0: 普通员工 (仅考勤), 1: 管理员 (可进入系统菜单)
    int role;               

    /// @brief  所属部门 ID (关联 DeptInfo.id)
    int dept_id;            

    ///绑定的默认班次ID
    int default_shift_id; 

    // 部门名称 (用于UI显示和报表，数据库不直接存，靠联表查询获取)
    std::string dept_name;

    /// @brief 人脸特征数据 (二进制流)
    /// @details 对应数据库中的 BLOB 字段，存储编码后的 JPG 图片数据
    std::vector<uchar> face_feature; 

    /// @brief 注册员工的人脸图片路径
    std::string avatar_path;
    
    /// @brief 指纹特征数据 (二进制流)
    std::vector<uint8_t> fingerprint_feature;

    /// @brief 职位信息 (用于报表显示)
    std::string position;
};

/**
 * @brief 考勤记录结构体 (视图模型)
 * @details 包含打卡时的详细信息，已关联查询出姓名和部门名
 */
struct AttendanceRecord {
    /// @brief 记录流水号
    int id;
    
    /// @brief 关联的用户ID
    int user_id;
    
    /// @brief 用户姓名 (关联查询结果)
    std::string user_name;  
    
    /// @brief 部门名称 (关联查询结果)
    std::string dept_name;  
    
    /// @brief 打卡时间戳 (秒级)
    long long timestamp;
    
    /// @brief 考勤状态 
    /// @note 0:正常, 1:迟到, 2:早退, 3:加班, 4:缺卡
    int status;             
    
    /// @brief 现场抓拍图片的文件路径
    std::string image_path;

    /// @brief 迟到分钟数 (用于报表计算)
    int minutes_late;

    /// @brief 早退分钟数 (用于报表计算)
    int minutes_early;
};

// 用于查询系统信息的结构体
struct SystemStats {
    int total_employees;    // 员工注册数 (总人数)
    int total_admins;       // 管理员注册数
    int total_faces;        // 人脸注册数
    int total_fingerprints; // 指纹注册数
    int total_cards;        // 卡号注册数
};

// ================= 核心接口声明 =================

/**
 * @brief 初始化数据层
 * @details 连接数据库，并自动创建所有必要的表结构 (users, departments, shifts, attendance, rules)
 * @return true 初始化成功
 * @return false 初始化失败 (如文件权限问题、SQL错误)
 */
bool data_init();

// [辅助函数] 简单哈希转换
std::string db_hash_password(const std::string& raw_pwd);

/**
 * @brief  数据播种 (Seeding)
 * @details 检查数据库是否为空，若为空则插入默认的部门、班次和管理员用户。
 * 通常在 data_init() 成功后自动调用。
 * @return true 播种成功或无需播种
 */
bool data_seed();

/**
 * @brief 关闭数据层
 * * 释放数据库连接句柄及相关资源。
 * 建议在程序退出前 (如 main 函数末尾) 显式调用。
 */
void data_close();

// ================= 1. 部门管理接口 (Department DAO) =================

/**
 * @brief 添加新部门
 * @param dept_name 部门名称 (需唯一)
 * @return true 添加成功
 */
bool db_add_department(const std::string& dept_name);

/**
 * @brief 获取所有部门列表
 * @return std::vector<DeptInfo> 部门列表数组
 */
std::vector<DeptInfo> db_get_departments();

/**
 * @brief 删除指定部门
 * @note 删除部门后，原属于该部门的员工其 dept_id 将被置为 NULL (外键约束 SET NULL)
 * @param dept_id 待删除的部门ID
 * @return true 删除成功
 */
bool db_delete_department(int dept_id);

// ================= 2. 班次管理接口 (Shift DAO) =================

/**
 * @brief 更新班次时间 (支持3个时段)
 * @param shift_id 要修改的班次ID
 * @param s1_start 时段1开始 "HH:MM"
 * @param s1_end   时段1结束
 * @param s2_start 时段2开始 (若无则传 "")
 * @param s2_end   时段2结束 (若无则传 "")
 * @param s3_start 时段3开始 (若无则传 "")
 * @param s3_end   时段3结束 (若无则传 "")
 * @param cross_day 是否跨天
 */
bool db_update_shift(int shift_id, 
                     const std::string& s1_start, const std::string& s1_end,
                     const std::string& s2_start, const std::string& s2_end,
                     const std::string& s3_start, const std::string& s3_end,
                     int cross_day);

/**
 * @brief 获取所有班次列表
 * @return std::vector<ShiftInfo> 班次列表数组
 */
std::vector<ShiftInfo> db_get_shifts();

/**
 * @brief 根据班次 ID 获取班次详细信息
 * @param shift_id 班次的数据库 ID
 * @return 成功返回班次信息，如果找不到则返回 std::nullopt
 */
std::optional<ShiftInfo> db_get_shift_info(int shift_id);

/**
 * @brief 创建新班次
 * @param name 班次名称 (如 "夜班")
 * @param start 上班时间 "22:00"
 * @param end 下班时间 "06:00"
 * @param cross_day 是否跨天 (1=是)
 * @return int 新班次ID
 */
int db_add_shift(const std::string& name, 
                 const std::string& s1_start, const std::string& s1_end,
                 const std::string& s2_start, const std::string& s2_end,
                 const std::string& s3_start, const std::string& s3_end,
                 int cross_day);

/**
 * @brief 删除班次
 * @param shift_id 待删除的班次ID
 */
bool db_delete_shift(int shift_id);

/**
 * @brief 获取全局考勤规则配置
 * @return RuleConfig 包含迟到/早退阈值
 */
RuleConfig db_get_global_rules();

/**
 * @brief 更新全局考勤规则配置
 * @param config 规则配置结构体
 * @return true 更新成功
 */
bool db_update_global_rules(const RuleConfig& config);

/** 获取所有定时响铃配置 
 * @return std::vector<BellSchedule> 响铃列表
 */
std::vector<BellSchedule> db_get_all_bells();

/**
 * @brief 更新单个定时响铃配置
 * @param bell 响铃配置结构体
 * @return true 更新成功
 */
bool db_update_bell(const BellSchedule& bell);

// ================= 3. 用户管理接口 (User DAO) =================

/**
 * @brief 注册新用户 (全量信息)
 * @details 将用户信息及人脸特征写入 `users` 表。
 * @param info 用户基本信息 (name, password, card_id, role, dept_id)
 * @param face_img 人脸图像 (将被编码为 BLOB 存入 info.face_feature)
 * @return int 成功返回生成的新工号(ID)，失败返回 -1
 */
int db_add_user(const UserData& info, const cv::Mat& face_img);

/**
 * @brief 批量导入/同步员工数据 (用于 U盘/网络批量同步)
 * @details 使用 SQLite 事务加速处理。若工号已存在则覆盖更新，若不存在则新增。
 * @param users_list 包含多个员工信息的列表
 * @return 全部成功返回 true，发生中途错误则自动回滚并返回 false
 */
bool db_batch_add_users(const std::vector<UserData>& users_list);

/**
 * @brief 删除用户
 * @note 将级联删除该用户的考勤记录 (CASCADE)
 * @param user_id 待删除的工号
 * @return true 删除成功
 */
bool db_delete_user(int user_id);

/**
 * @brief 获取单个用户详情
 * @details 包含基本信息。人脸特征数据按需加载（当前实现默认不加载BLOB以优化性能，如需加载请在cpp中调整SQL）。
 * @param user_id 工号
 * @return UserData 用户信息结构体 (若不存在，ID为0)
 */
std::optional<UserData> db_get_user_info(int user_id);

/**
 * @brief 获取所有用户基础数据
 * @details 专门用于业务层(FaceDemo)在系统启动时加载数据以训练识别模型。
 * 此时返回的 UserData 中应当包含 `face_feature` 数据。
 * @return std::vector<UserData> 包含人脸数据的用户列表
 */
std::vector<UserData> db_get_all_users();

/**
 * @brief 获取所有用户的基础信息 (用于报表和UI列表)
 * @details 包含部门名称，但不包含人脸数据(BLOB)，轻量高效。
 * @return std::vector<UserData> 用户列表
 */
std::vector<UserData> db_get_all_users_info();

/**
 * @brief 给用户指定班次 (排班)
 * @param user_id 用户ID
 * @param shift_id 班次ID (传 0 或 -1 代表“未排班/使用默认规则”)
 */
bool db_assign_user_shift(int user_id, int shift_id);

/**
 * @brief 获取用户绑定的班次信息
 * @return ShiftInfo 班次信息 (如果未绑定，返回ID=0的空结构体)
 */
ShiftInfo db_get_user_shift(int user_id);

/**
 * @brief 更新用户基本信息 (不包含密码和人脸)
 * @param user_id 要修改的用户ID
 * @param name 新姓名
 * @param dept_id 新部门ID
 * @param privilege 新权限 (0:普通用户, 1:管理员)
 * @param card_id 新卡号
 * @return true 更新成功
 */
bool db_update_user_basic(int user_id, const std::string& name, int dept_id, int privilege, const std::string& card_id);

/**
 * @brief 单独修改用户人脸
 * @param user_id 用户ID
 * @param  face_image新人脸 
 * @return true 修改成功
 */
bool db_update_user_face(int user_id, const cv::Mat& face_image);

/**
 * @brief 单独修改用户密码
 * @param user_id 用户ID
 * @param new_raw_password 新密码 (明文，内部会自动哈希)
 * @return true 修改成功
 */
bool db_update_user_password(int user_id, const std::string& new_raw_password);

/**
 * @brief 单独修改/录入用户指纹特征
 * @details 对应考勤机上按压指纹录入的功能。将采集到的指纹特征(二进制)更新到数据库。
 * @param user_id 用户的工号(ID)
 * @param fingerprint_data 指纹特征数据的二进制序列
 * @return 成功返回 true，失败返回 false
 */
bool db_update_user_fingerprint(int user_id, const std::vector<uint8_t>& fingerprint_data);

/**
 * @brief  获取所有用户的基础信息 (仅ID和姓名)
 * @details 专门用于系统启动时快速构建 ID->Name 映射表，不加载图片数据，速度极快。
 * @return std::vector<UserData> 用户列表 (仅填充 id 和 name 字段)
 */
std::vector<UserData> db_get_all_users_light();

// ================= 4. 考勤记录接口 (Attendance DAO) =================

/**
 * @brief 记录考勤
 * @details 保存抓拍图片到磁盘，并将记录写入 `attendance` 表
 * @param user_id 识别到的用户ID
 * @param shift_id 当前匹配的班次ID (无班次可传0)
 * @param image 现场抓拍图
 * @param status 考勤状态码 (0:正常)
 * @return true 记录成功
 */
bool db_log_attendance(int user_id, int shift_id, const cv::Mat& image, int status);

/**
 * @brief 查询考勤记录
 * @details 支持按时间段筛选，结果包含用户名和部门名
 * @param start_ts 开始时间戳
 * @param end_ts 结束时间戳
 * @return std::vector<AttendanceRecord> 记录列表
 */
std::vector<AttendanceRecord> db_get_records(long long start_ts, long long end_ts);

// 获取指定用户的最后一次打卡时间戳，如果没有记录返回 0
time_t db_getLastPunchTime(int user_id);

/**
 * @brief 按工号和时间段查询个人的考勤记录 
 * @param user_id 用户的工号(ID)
 * @param start_ts 起始时间戳 (秒)
 * @param end_ts 结束时间戳 (秒)
 * @return 考勤记录列表 (包含姓名和部门名，按时间升序排列)
 */
std::vector<AttendanceRecord> db_get_records_by_user(int user_id, long long start_ts, long long end_ts);

/**
 * @brief 自动清理过期的考勤抓拍图片
 * @param days_old 超过多少天的图片将被清理 (默认 30 天)
 * @return 成功清理的图片数量
 */
int db_cleanup_old_attendance_images(int days_old = 30);


// ================= 5. 数据库事务接口 =================

/**
 * @brief 开启数据库事务 (用于批量操作加速)
 */
bool db_begin_transaction();

/**
 * @brief 提交数据库事务
 */
bool db_commit_transaction();

// ================= 6. 排班管理接口 (Schedule DAO) =================

/**
 * @brief 设置部门的周排班
 * @param dept_id 部门ID
 * @param day_of_week 0=周日, 1=周一, ..., 6=周六
 * @param shift_id 班次ID (传0代表休息/无班次)
 */
bool db_set_dept_schedule(int dept_id, int day_of_week, int shift_id);

/**
 * @brief 设置个人的特定日期排班 (最高优先级)
 * @param user_id 用户ID
 * @param date_str 日期字符串 "YYYY-MM-DD"
 * @param shift_id 班次ID (传0代表当天休息)
 */
bool db_set_user_special_schedule(int user_id, const std::string& date_str, int shift_id);

/**
 * @brief  根据日期智能获取用户当天的班次
 * @details 优先级逻辑:
 * 1. 检查 `user_schedule` (个人特殊排班)
 * 2. 若无，检查 `dept_schedule` (部门周排班)
 * 3. 若无，返回 users 表中的 default_shift_id
 * * @param user_id 用户ID
 * @param timestamp 查询的时间戳 (秒)
 * @return ShiftInfo 班次信息 (若当天休息或未排班，返回ID=0的空对象)
 */
std::optional<ShiftInfo> db_get_user_shift_smart(int user_id, long long timestamp);

// ================= 更新/删除数据 =================

/**
 * @brief 获取最后保存图像的ID
 * @return 最后保存图像的ID，失败返回 -1
 */
long long data_getLastImageID();// 获取最后保存图像的ID

/**
 * @brief  清空所有考勤记录
 * @details 删除 attendance 表数据，清空 captured_images 目录
 */
bool db_clear_attendance();

/**
 * @brief  清空所有员工数据
 * @details 删除 users 表数据及其关联的图片文件
 */
bool db_clear_users();

/**
 * @brief  恢复出厂设置
 * @details 清除所有数据库数据和图片，重置系统
 */
bool db_factory_reset();

/**
 * @brief 查询系统信息
 */
SystemStats db_get_system_stats();

// ================= 系统全局配置接口 =================

/**
 * @brief 获取系统全局配置值
 * @param key 配置项的键名 (如 "device_id", "volume", "company_name")
 * @param default_value 如果数据库里没有存这个键，默认返回的值
 * @return 对应的字符串值
 */
std::string db_get_system_config(const std::string& key, const std::string& default_value = "");

/**
 * @brief 设置系统全局配置值 (存在则更新，不存在则插入)
 * @param key 配置项的键名
 * @param value 配置项的值 (统一转为字符串存储，业务层读取时再按需转回 int)
 * @return 是否设置成功
 */
bool db_set_system_config(const std::string& key, const std::string& value);

// ================= 全局节假日管理接口 =================

/**
 * @brief 设置全局节假日 (新增或修改)
 * @param date_str 日期字符串，格式必须统一，如 "YYYY-MM-DD"
 * @param holiday_name 节日名称，如 "国庆节"
 * @return 是否设置成功
 */
bool db_set_holiday(const std::string& date_str, const std::string& holiday_name);

/**
 * @brief 删除指定的全局节假日 (例如取消放假)
 * @param date_str 日期字符串 "YYYY-MM-DD"
 * @return 是否删除成功
 */
bool db_delete_holiday(const std::string& date_str);

/**
 * @brief 检查某天是否为全局节假日
 * @param date_str 日期字符串 "YYYY-MM-DD"
 * @return 如果是，返回节日名称(如"中秋节")；如果不是，返回 std::nullopt
 */
std::optional<std::string> db_get_holiday(const std::string& date_str);

// ================= 考勤设置与排班管理接口 =================

/**
 * @brief 部门排班特殊值定义
 * 对应业务文档规则："填写的数字代表班次号(1-10)，留空或0代表节假日"
 */
namespace ScheduleConstants {
    const int HOLIDAY = 0;          // 0代表节假日
    const int MIN_SHIFT_ID = 1;     // 最小班次号
    const int MAX_SHIFT_ID = 10;    // 最大班次号（业务文档限制）
    
    // 星期几枚举（与数据库day_of_week字段对应）
    enum DayOfWeek {
        SUNDAY = 0,
        MONDAY = 1,
        TUESDAY = 2,
        WEDNESDAY = 3,
        THURSDAY = 4,
        FRIDAY = 5,
        SATURDAY = 6
    };
}

/**
 * @brief 检查时间字符串是否表示空值（无考勤要求）
 * @param time_str 时间字符串
 * @return true 表示空值（"--:--"或空字符串）
 * @note 业务文档规定："--:--"代表无考勤要求
 */
bool is_time_empty(const std::string& time_str);

/**
 * @brief 标准化时间字符串
 * @param time_str 原始时间字符串
 * @return 标准化后的时间字符串，空值返回"--:--"
 * @note 用于数据持久化前的格式化
 */
std::string normalize_time_string(const std::string& time_str);

/**
 * @brief 批量导入部门排班数据
 * @param schedules 部门排班数据向量
 * @return 成功导入的记录数
 * @note 对应业务文档中的部门排班表导入功能
 */
int db_import_dept_schedules(const std::vector<DeptScheduleEntry>& schedules);

/**
 * @brief 获取部门完整排班视图
 * @param dept_id 部门ID
 * @return 部门一周排班视图
 */
DeptScheduleView db_get_dept_schedule_view(int dept_id);

/**
 * @brief 获取所有班次（限制最多10个）
 * @return 班次列表，按ID排序
 * @note 业务文档限制最多10个班次
 */
std::vector<ShiftInfo> db_get_all_shifts_limited();

// ================= 报表辅助批量查询接口 =================

/**
 * @brief 根据时间段批量获取全公司的打卡记录 (用于生成月度总表)
 * @details 使用 LEFT JOIN 一次性查出用户姓名和部门名称，避免业务层产生 N+1 查询问题。
 * @param start_ts 起始时间戳 (秒)
 * @param end_ts 结束时间戳 (秒)
 * @return 包含所有人打卡信息的记录列表，默认按部门和工号排序
 */
std::vector<AttendanceRecord> db_get_all_records_by_time(long long start_ts, long long end_ts);

/**
 * @brief 获取某部门下的所有用户列表 (用于按部门导出报表)
 * @param dept_id 部门ID (传 0 可以表示获取全公司无部门的散客，或者你可以自定义逻辑)
 * @return 属于该部门的用户列表
 */
std::vector<UserData> db_get_users_by_dept(int dept_id);

#endif // DB_STORAGE_H