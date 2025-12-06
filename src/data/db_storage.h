/**
 * @file db_storage.h
 * @brief 数据层核心接口 - 负责数据库管理与图像持久化
 * @details 包含 SQLite3 数据库的初始化、资源释放以及图像数据的二进制存储功能。
 * @author SmartAttendance Team
 * @version 1.0
 * @date 2023-10-27
 */

#ifndef DB_STORAGE_H
#define DB_STORAGE_H

#include <opencv2/core.hpp>

/**
 * @brief 初始化数据层
 * * 连接或创建名为 "attendance.db" 的 SQLite 数据库文件。
 * 如果数据库不存在，会自动创建，并初始化所需的数据表 (processed_images)。
 * * @return true  初始化成功 (数据库连接正常且表创建成功)
 * @return false 初始化失败 (可能是文件权限、磁盘空间或 SQL 语法错误)
 * * @note 此函数应在程序启动时尽早调用。
 */
bool data_init();

/**
 * @brief 关闭数据层
 * * 释放数据库连接句柄及相关资源。
 * 建议在程序退出前 (如 main 函数末尾) 显式调用。
 */
void data_close();

/**
 * @brief 保存图像数据到数据库
 * * 将传入的 OpenCV 图像矩阵编码为 JPG 格式的二进制流 (BLOB)，
 * 并连同当前系统时间戳一起写入到数据库中。
 * * @param[in] image 待保存的图像帧 (cv::Mat)。建议传入裁剪后的人脸区域以节省空间。
 * * @return true  保存成功
 * @return false 保存失败 (如数据库未初始化、图像为空或 SQL 执行错误)
 * * @note 这是一个 IO 密集型操作，建议避免在 UI 渲染线程中高频调用，以免造成界面卡顿。
 */
bool data_saveImage(const cv::Mat& image);

long long data_getLastImageID();// 获取最后保存图像的ID

#endif // DB_STORAGE_H