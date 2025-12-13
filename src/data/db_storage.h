/**
 * @file db_storage.h
 * @brief 数据层核心接口 (混合存储版)
 * @details 负责数据库索引管理与图像文件持久化。
 * @version 1.2 (File System Storage)
 */

#ifndef DB_STORAGE_H
#define DB_STORAGE_H

#include <opencv2/core.hpp>
#include <string> // [新增] 需要处理字符串
#include <vector>

// [新增] 用户信息结构体
struct UserData {
    int id;
    std::string name;
    std::vector<uchar> face_feature; // 用于存储二进制数据 (BLOB)
};

// [新增] 考勤记录结构体
struct AttendanceRecord {
    int id;
    int user_id;
    std::string image_path; // 存储文件路径
    long long timestamp;
};

/**
 * @brief 初始化数据层
 * * 连接 SQLite 数据库 (attendance.db)。
 * * 创建 users 表 (BLOB存储) 和 attendance_logs 表 (路径存储)
 * * @return true 初始化成功
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