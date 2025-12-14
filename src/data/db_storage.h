/**
 * @file db_storage.h
 * @brief 数据层核心接口 (Phase 02 Update)
 * @details 包含用户、部门、班次等核心实体定义
 */

#ifndef DB_STORAGE_H
#define DB_STORAGE_H

#include <opencv2/core.hpp>
#include <string> // [新增] 需要处理字符串
#include <vector>

// [Epic 2.1 新增] 部门信息
struct DeptInfo {
    int id;
    std::string name;
};

// [Epic 2.1 新增] 班次信息
struct ShiftInfo {
    int id;
    std::string name;
    std::string start_time; // 格式 "HH:MM" e.g. "09:00"
    std::string end_time;   // 格式 "HH:MM" e.g. "18:00"
    int cross_day;          // 0: 当天, 1: 跨天
};

// [Epic 2.1 修改] 用户信息升级 (对应 users 表)
struct UserData {
    int id;                 // 工号
    std::string name;       // 姓名
    std::string password;   // [新增] 密码
    std::string card_id;    // [新增] 卡号
    int role;               // [新增] 权限 0:普通, 1:管理员
    int dept_id;            // [新增] 部门ID
    std::vector<uchar> face_feature; // 人脸特征(BLOB)
};

// [Epic 2.1 修改] 考勤记录升级 (对应 attendance 表)
struct AttendanceRecord {
    int id;
    int user_id;
    int shift_id;           // [新增] 关联班次
    std::string image_path;
    long long timestamp;
    int status;             // [新增] 0:正常, 1:迟到, 2:早退...
};

/**
 * @brief 初始化数据层 (Phase 02: 完整Schema构建)
 * 创建 users, departments, shifts, attendance_rules, attendance 五张表
 */
bool data_init();

/**
 * @brief 关闭数据层
 * * 释放数据库连接句柄及相关资源。
 * 建议在程序退出前 (如 main 函数末尾) 显式调用。
 */
void data_close();

// [修改] 废弃原有的 data_saveImage，改为以下两个新接口：

/**
 * @brief 注册新用户（混合存储：数据存DB）
 * @param name 用户名
 * @param face_image 人脸图像（将转换为二进制存入BLOB）
 * @return int 返回新用户的ID，失败返回 -1
 */
int data_registerUser(const std::string& name, const cv::Mat& face_image);

/**
 * @brief 保存考勤记录（混合存储：图片存磁盘，路径存DB）
 * @param user_id 识别到的用户ID
 * @param image 当前抓拍的现场图
 * @return true 保存成功
 */
bool data_saveAttendance(int user_id, const cv::Mat& image);

/**
 * @brief 获取所有用户数据（用于系统启动时训练模型）
 */
std::vector<UserData> data_getAllUsers();

#endif // DB_STORAGE_H