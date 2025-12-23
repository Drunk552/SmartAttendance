/**
 * @file db_storage.h
 * @brief 数据层核心接口 (Phase 02 - DAO Layer)
 * @details 封装数据库操作，提供部门、班次、用户及考勤记录的增删改查（DAO）接口。
 * @version 2.0
 * @date 2025-12-14
 */

#ifndef DB_STORAGE_H
#define DB_STORAGE_H

#ifdef __cplusplus
#include <opencv2/core.hpp>
#include <string> // [新增] 需要处理字符串
#include <vector>
#include <utility> // for std::pair

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
 * @brief 班次信息结构体
 * @details 对应数据库 `shifts` 表，定义上下班时间规则
 */
struct ShiftInfo {
    /// @brief 班次ID (数据库自增主键)
    int id;
    
    /// @brief 班次名称 (e.g. "早班")
    std::string name;

    /// @brief 时段 1 (e.g. 09:00 - 12:00)
    std::string s1_start; 
    std::string s1_end;

    /// @brief 时段 2 (e.g. 13:00 - 18:00)
    std::string s2_start; 
    std::string s2_end;

    /// @brief 时段 3 (e.g. 19:00 - 21:00, 可为空)
    std::string s3_start; 
    std::string s3_end;  
    
    /// @brief 是否跨天 (0: 当天, 1: 次日)
    int cross_day;          
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

    /// @brief [新增] 登录密码 (用于键盘输入验证)
    std::string password;   

    /// @brief [新增] IC/ID卡号 (用于刷卡验证)
    std::string card_id;    

    /// @brief [新增] 权限等级
    /// @note 0: 普通员工 (仅考勤), 1: 管理员 (可进入系统菜单)
    int role;               

    /// @brief [新增] 所属部门 ID (关联 DeptInfo.id)
    int dept_id;            

    ///绑定的默认班次ID
    int default_shift_id; 

    // [新增] 部门名称 (用于UI显示和报表，数据库不直接存，靠联表查询获取)
    std::string dept_name;

    /// @brief 人脸特征数据 (二进制流)
    /// @details 对应数据库中的 BLOB 字段，存储编码后的 JPG 图片数据
    std::vector<uchar> face_feature; 
    
    /// @brief 指纹特征数据 (二进制流)
    std::vector<uint8_t> fingerprint_feature;
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
};

// ================= 核心接口声明 =================
#endif

/**
 * @brief 初始化数据层
 * @details 连接数据库，并自动创建所有必要的表结构 (users, departments, shifts, attendance, rules)
 * @return true 初始化成功
 * @return false 初始化失败 (如文件权限问题、SQL错误)
 */
bool data_init();

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
UserData db_get_user_info(int user_id);

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
 * @brief 单独修改用户密码
 * @param user_id 用户ID
 * @param new_raw_password 新密码 (明文，内部会自动哈希)
 * @return true 修改成功
 */
bool db_update_user_password(int user_id, const std::string& new_raw_password);

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
 * @brief [核心] 根据日期智能获取用户当天的班次
 * @details 优先级逻辑:
 * 1. 检查 `user_schedule` (个人特殊排班)
 * 2. 若无，检查 `dept_schedule` (部门周排班)
 * 3. 若无，返回 users 表中的 default_shift_id
 * * @param user_id 用户ID
 * @param timestamp 查询的时间戳 (秒)
 * @return ShiftInfo 班次信息 (若当天休息或未排班，返回ID=0的空对象)
 */
ShiftInfo db_get_user_shift_smart(int user_id, long long timestamp);

// ================= 兼容性接口 (Legacy Support) =================

/**
 * @brief [已弃用] 简单注册接口
 * @note 仅用于兼容旧测试代码，建议使用 db_add_user
 * @brief 注册新用户（混合存储：数据存DB）
 * @param name 用户名
 * @param face_image 人脸图像（将转换为二进制存入BLOB）
 * @return int 返回新用户的ID，失败返回 -1
 */
int data_registerUser(const std::string& name, const cv::Mat& face_image);

/**
 * @brief [已弃用] 简单考勤接口
 * @note 仅用于兼容旧测试代码，建议使用 db_log_attendance
 * @brief 保存考勤记录（混合存储：图片存磁盘，路径存DB）
 * @param user_id 识别到的用户ID
 * @param image 当前抓拍的现场图
 * @return true 保存成功
 */
bool data_saveAttendance(int user_id, const cv::Mat& image);

/**
 * @brief 获取所有用户数据（用于系统启动时训练模型）
 */
// data_getAllUsers 已被 db_get_all_users 替代，但在 business_init 中被调用，保留声明
// 注意：Phase 1 的代码使用的是 std::vector<UserData> data_getAllUsers()
// 既然 UserData 已经改回来了，我们可以直接复用上面的 db_get_all_users
// 或者保留别名：
inline std::vector<UserData> data_getAllUsers() {
    return db_get_all_users();
}
#ifdef __cplusplus
/**
 * @brief 保存图像数据到数据库
 * * 将传入的 OpenCV 图像矩阵编码为 JPG 格式的二进制流 (BLOB)，
 * 并连同当前系统时间戳一起写入到数据库中。
 * * @param[in] image 待保存的图像帧 (cv::Mat)。建议传入裁剪后的人脸区域以节省空间。
 * * @return true  保存成功
 * * @return false 保存失败 (如数据库未初始化、图像为空或 SQL 执行错误)
 * * @note 这是一个 IO 密集型操作，建议避免在 UI 渲染线程中高频调用，以免造成界面卡顿。
 */
bool data_saveImage(const cv::Mat& image);
#endif

long long data_getLastImageID();// 获取最后保存图像的ID

/**
 * @brief [Epic 4.3] 清空所有考勤记录
 * @details 删除 attendance 表数据，清空 captured_images 目录
 */
bool db_clear_attendance();

/**
 * @brief [Epic 4.3] 恢复出厂设置
 * @details 清除所有数据库数据和图片，重置系统
 */
bool db_factory_reset();

#endif // DB_STORAGE_H