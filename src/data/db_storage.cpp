/**
 * @file db_storage.cpp
 * @brief 数据层接口的具体实现
 */

#include "db_storage.h"
#include <sqlite3.h>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <vector>
#include <ctime>

// 数据库连接句柄 (静态全局变量，仅本文件可见)
static sqlite3* db = nullptr;

bool data_init() {
    // 1. 打开/创建数据库文件 attendance.db
    int rc = sqlite3_open("attendance.db", &db);
    if (rc) {
        std::cerr << "[Data] Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 2. 创建表 processed_images
    // 字段: id (自增主键), image_data (二进制图片), timestamp (时间戳)
    const char* sql = "CREATE TABLE IF NOT EXISTS processed_images ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "image_data BLOB, "
                      "timestamp INTEGER);";
    
    char* zErrMsg = 0;
    rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[Data] SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return false;
    }
    
    std::cout << "[Data] Database initialized successfully." << std::endl;
    return true;
}

void data_close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
        std::cout << "[Data] Database connection closed." << std::endl;
    }
}

bool data_saveImage(const cv::Mat& image) {
    if (!db) {
        std::cerr << "[Data] Error: Database not initialized!" << std::endl;
        return false;
    }
    if (image.empty()) {
        std::cerr << "[Data] Error: Input image is empty!" << std::endl;
        return false;
    }

    // 1. 将 OpenCV Mat 编码为 JPG 格式的字节流
    std::vector<uchar> buf;
    // 使用 .jpg 格式压缩存储，节省空间；若需无损可改用 .png
    // 参数3可以传入压缩质量参数，这里使用默认值
    if (!cv::imencode(".jpg", image, buf)) {
        std::cerr << "[Data] Error: Image encoding failed!" << std::endl;
        return false;
    }

    // 2. 准备 SQL 插入语句
    const char* sql = "INSERT INTO processed_images (image_data, timestamp) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    
    // 预编译 SQL (防止 SQL 注入，提高性能)
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        std::cerr << "[Data] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 3. 绑定参数
    // 参数1: image_data (BLOB) - 这里的 1 代表第一个问号
    sqlite3_bind_blob(stmt, 1, buf.data(), (int)buf.size(), SQLITE_STATIC);
    // 参数2: timestamp (INTEGER) - 这里的 2 代表第二个问号
    sqlite3_bind_int64(stmt, 2, std::time(nullptr));

    // 4. 执行 SQL
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "[Data] Execution failed: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt); // 即使失败也要释放 stmt
        return false;
    }

    // 获取刚插入的 ID，用于日志记录
    long long last_id = sqlite3_last_insert_rowid(db);
    std::cout << "[Data] Image saved successfully! ID: " << last_id 
              << ", Size: " << buf.size() << " bytes" << std::endl;

    // 5. 清理语句句柄
    sqlite3_finalize(stmt);
    return true;
}