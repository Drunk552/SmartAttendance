#include "infrastructure/logging/logger.h"
/**
 * @file
 * @brief 承接旧员工 SQLite DAO。
 */

#include "data/db_storage.h"
#include "storage/sqlite/legacy_db_internal.h"

#include <sqlite3.h>
#include <opencv2/imgcodecs.hpp>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// ================= 3. 用户管理 DAO  =================

int db_add_user(const UserData& user, const cv::Mat& face_image) {
    // 1. 无需锁的耗时操作：保存注册时人脸照片到磁盘
    std::string path_str = "";
    if (!face_image.empty()) {
        // 确保注册时人脸照片文件夹存在
        if (!fs::exists(AVATAR_DIR)) fs::create_directories(AVATAR_DIR);

        // 用时间戳或随机数+名字作为文件名
        long long now = std::time(nullptr);
        std::string fname = std::to_string(now) + "_" + user.name + ".jpg";
        fs::path p = fs::path(AVATAR_DIR) / fname;

        try {
            if (cv::imwrite(p.string(), face_image)) {
                path_str = p.string();
            }
        } catch (...) {
            SA_LOG_ERROR_STREAM() << "[Data] Save Avatar Image Failed." << std::endl;
        }
    }

    // 2. 核心数据库写入：精确加锁
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    // 注意：这里的 SQL 加入了 avatar_path
    const char* sql =
        "INSERT INTO users (name, password, card_id, privilege, avatar_path, dept_id) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt.get(), 1, user.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, user.password.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, user.card_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.get(), 4, user.role);

        // 存路径
        if (!path_str.empty()) {
            sqlite3_bind_text(stmt.get(), 5, path_str.c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt.get(), 5);
        }

        if (user.dept_id > 0) {
            sqlite3_bind_int(stmt.get(), 6, user.dept_id);
        } else {
            sqlite3_bind_null(stmt.get(), 6);
        }

        if (sqlite3_step(stmt.get()) == SQLITE_DONE) {
            return sqlite3_last_insert_rowid(db);
        }
    }

    return -1;
}

//批量导入/同步员工数据 (用于 U盘/网络批量同步)
bool db_batch_add_users(const std::vector<UserData>& users_list) {
    if (users_list.empty()) return true;

    // 1. 获取排他锁，防止批量写入时与其他线程的读写操作冲突
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    // 2. 开启事务 (BEGIN TRANSACTION)，极大地加速批量插入
    char* zErrMsg = 0;
    if (sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &zErrMsg) != SQLITE_OK) {
        SA_LOG_ERROR_STREAM() << "[Data] Begin Transaction Failed: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return false;
    }

    // 3. 准备 SQL 语句
    // 使用 INSERT OR REPLACE：如果 id 已存在，则覆盖更新；如果不存在，则新增。
    // 这里包含你 sql_users 表中除了自增ID外的所有关键业务字段
    const char* sql = "INSERT OR REPLACE INTO users "
                      "(id, name, password, card_id, privilege, face_feature, avatar_path, fingerprint_data, dept_id, default_shift_id) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        SA_LOG_ERROR_STREAM() << "[Data] Prepare Batch Add Users Failed: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0); // 发生错误，回滚
        return false;
    }

    bool success = true;

    // 4. 循环绑定数据并执行
    for (const auto& user : users_list) {
        // 绑定 1: id (工号)
        sqlite3_bind_int(stmt.get(), 1, user.id);

        // 绑定 2: name (姓名)
        sqlite3_bind_text(stmt.get(), 2, user.name.c_str(), -1, SQLITE_STATIC);

        // 绑定 3: password (登录密码)
        if (user.password.empty()) sqlite3_bind_null(stmt.get(), 3);
        else sqlite3_bind_text(stmt.get(), 3, user.password.c_str(), -1, SQLITE_STATIC);

        // 绑定 4: card_id (IC/ID卡号)
        if (user.card_id.empty()) sqlite3_bind_null(stmt.get(), 4);
        else sqlite3_bind_text(stmt.get(), 4, user.card_id.c_str(), -1, SQLITE_STATIC);

        // 绑定 5: privilege (对应 UserData 里的 role)
        sqlite3_bind_int(stmt.get(), 5, user.role);

        // 绑定 6: face_feature (对应 UserData 里的 face_feature 二进制流)
        if (user.face_feature.empty()) {
            sqlite3_bind_null(stmt.get(), 6);
        } else {
            sqlite3_bind_blob(stmt.get(), 6, user.face_feature.data(), user.face_feature.size(), SQLITE_STATIC);
        }

        // 绑定 7: avatar_path (人脸图片路径)
        if (user.avatar_path.empty()) sqlite3_bind_null(stmt.get(), 7);
        else sqlite3_bind_text(stmt.get(), 7, user.avatar_path.c_str(), -1, SQLITE_STATIC);

        // 绑定 8: fingerprint_data (对应 UserData 里的 fingerprint_feature 二进制流)
        if (user.fingerprint_feature.empty()) {
            sqlite3_bind_null(stmt.get(), 8);
        } else {
            sqlite3_bind_blob(stmt.get(), 8, user.fingerprint_feature.data(), user.fingerprint_feature.size(), SQLITE_STATIC);
        }

        // 绑定 9: dept_id (部门ID)
        if (user.dept_id <= 0) sqlite3_bind_null(stmt.get(), 9);
        else sqlite3_bind_int(stmt.get(), 9, user.dept_id);

        // 绑定 10: default_shift_id (默认班次ID)
        if (user.default_shift_id <= 0) sqlite3_bind_null(stmt.get(), 10);
        else sqlite3_bind_int(stmt.get(), 10, user.default_shift_id);

        // 执行单条语句
        if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
            SA_LOG_ERROR_STREAM() << "[Data] Batch Insert Error on User ID " << user.id
                      << ": " << sqlite3_errmsg(db) << std::endl;
            success = false;
            break; // 出现错误，跳出循环
        }

        // 重置语句状态，以便下一次循环可以重新绑定新数据！
        sqlite3_clear_bindings(stmt.get());
        sqlite3_reset(stmt.get());
    }

    // 5. 根据执行结果提交或回滚
    if (success) {
        sqlite3_exec(db, "COMMIT;", 0, 0, 0); // 正式写入磁盘
        SA_LOG_INFO_STREAM() << "[Data] Successfully batch synced " << users_list.size() << " users." << std::endl;
    } else {
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0); // 放弃之前的所有更改
        SA_LOG_ERROR_STREAM() << "[Data] Batch sync failed, all changes rolled back." << std::endl;
    }

    return success;
}

//批量更新/导入员工的排班信息 (用于 U盘/网络批量同步)
bool db_batch_update_user_schedules(int year, int month, const std::vector<UserData>& users_list) {
    if (users_list.empty()) return true;
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    char* zErrMsg = 0;
    // 开启事务，加速写入
    if (sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &zErrMsg) != SQLITE_OK) {
        SA_LOG_ERROR("[Data Error] Begin Transaction Failed (Schedules): %s\n", zErrMsg ? zErrMsg : "Unknown");
        if (zErrMsg) sqlite3_free(zErrMsg);
        return false;
    }

    // 准备排班插入语句（有则覆盖，无则新增）
    const char* sql = "INSERT OR REPLACE INTO user_schedule (user_id, date_str, shift_id) VALUES (?, ?, ?);";
    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        SA_LOG_ERROR("[Data Error] Prepare Schedule Update Failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return false;
    }

    bool success = true;

    // 遍历每一个员工
    for (const auto& user : users_list) {
        // 遍历该员工的 1-31 天排班字典
        for (const auto& [day, shift_id] : user.monthly_schedule) {

            if (shift_id <= 0) continue; // 如果是 0，说明是休息，直接跳过不写数据库！

            // 组装标准的日期字符串，例如 "2026-03-01"
            char date_buf[16];
            snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d", year, month, day);

            // 绑定：用户ID、日期、班次ID
            sqlite3_bind_int(stmt.get(), 1, user.id);
            sqlite3_bind_text(stmt.get(), 2, date_buf, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt.get(), 3, shift_id); // 0表示休息

            // 执行单条插入
            if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
                SA_LOG_ERROR("[Data Error] Insert Schedule Error (User:%d, Date:%s): %s\n",
                       user.id, date_buf, sqlite3_errmsg(db));
                success = false;
                break;
            }
            sqlite3_clear_bindings(stmt.get());
            sqlite3_reset(stmt.get());
        }
        if (!success) break;
    }

    // 提交或回滚
    if (success) {
        sqlite3_exec(db, "COMMIT;", 0, 0, 0);
        SA_LOG_INFO("[Data] Successfully batch synced schedules for %zu users.\n", users_list.size());
    } else {
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
    }
    return success;
}

DbUserLookupResult db_find_user_info(int user_id) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    if (db == nullptr) {
        return {DbUserLookupStatus::ReadError, std::nullopt};
    }

    const char* sql =
        "SELECT u.id, u.name, u.password, u.card_id, u.privilege, u.dept_id, "
        "u.face_feature, u.fingerprint_data, d.name, u.avatar_path "
        "FROM users u "
        "LEFT JOIN departments d ON u.dept_id = d.id "
        "WHERE u.id=?;";

    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        SA_LOG_ERROR_STREAM() << "[Data] Get User Info SQL Error: " << sqlite3_errmsg(db) << std::endl;
        return {DbUserLookupStatus::ReadError, std::nullopt};
    }

    if (sqlite3_bind_int(stmt.get(), 1, user_id) != SQLITE_OK) {
        SA_LOG_ERROR_STREAM() << "[Data] Bind User Info SQL Error: " << sqlite3_errmsg(db) << std::endl;
        return {DbUserLookupStatus::ReadError, std::nullopt};
    }

    const int stepResult = sqlite3_step(stmt.get());
    if (stepResult == SQLITE_ROW) {
            UserData u; // 将对象定义移到这里，查到了才创建

            // [0] ID
            u.id = sqlite3_column_int(stmt.get(), 0);

            // [1] Name
            const char* name = (const char*)sqlite3_column_text(stmt.get(), 1);
            u.name = name ? name : "";

            // [2] Password
            const char* pwd = (const char*)sqlite3_column_text(stmt.get(), 2);
            u.password = pwd ? pwd : "";

            // [3] Card ID
            const char* card = (const char*)sqlite3_column_text(stmt.get(), 3);
            u.card_id = card ? card : "";

            // [4] Role (DB字段是 role)
            u.role = sqlite3_column_int(stmt.get(), 4);

            // [5] Dept ID
            u.dept_id = sqlite3_column_int(stmt.get(), 5);

            // [6] Face Data (人脸数据)
            const void* face_blob = sqlite3_column_blob(stmt.get(), 6);
            int face_bytes = sqlite3_column_bytes(stmt.get(), 6);
            if (face_blob && face_bytes > 0) {
                const uint8_t* ptr = (const uint8_t*)face_blob;
                u.face_feature.assign(ptr, ptr + face_bytes);
            }

            // [7] Fingerprint Data (指纹数据)
            const void* fp_blob = sqlite3_column_blob(stmt.get(), 7);
            int fp_bytes = sqlite3_column_bytes(stmt.get(), 7);
            if (fp_blob && fp_bytes > 0) {
                const uint8_t* ptr = (const uint8_t*)fp_blob;
                u.fingerprint_feature.assign(ptr, ptr + fp_bytes);
            }

            // [8] Dept Name (部门名称) - 修复部门不显示的问题
            const char* dname = (const char*)sqlite3_column_text(stmt.get(), 8);
            u.dept_name = dname ? dname : "Unknown"; // 如果没部门，显示Unknown

            // [9] Avatar Path (注册时人脸照片存储路径)
            const char* avatar = (const char*)sqlite3_column_text(stmt.get(), 9);
            u.avatar_path = avatar ? avatar : ""; // 如果数据库里存了路径就取出来，没有就是空字符串

        return {DbUserLookupStatus::Found, std::move(u)};
    }

    if (stepResult == SQLITE_DONE) {
        return {DbUserLookupStatus::NotFound, std::nullopt};
    }

    SA_LOG_ERROR_STREAM() << "[Data] Read User Info SQL Error: " << sqlite3_errmsg(db) << std::endl;
    return {DbUserLookupStatus::ReadError, std::nullopt};
}

std::optional<UserData> db_get_user_info(int user_id) {
    DbUserLookupResult result = db_find_user_info(user_id);
    if (result.status != DbUserLookupStatus::Found) {
        return std::nullopt;
    }
    return std::move(result.user);
}

bool db_delete_user(int user_id) {

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "DELETE FROM users WHERE id=?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt.get(), 1, user_id);
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    return ok;
}

std::vector<UserData> db_get_all_users() {

    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    std::vector<UserData> users;
    const char* sql = "SELECT u.id, u.name, u.dept_id, u.privilege, u.password, u.card_id, u.face_feature, d.name, u.avatar_path "
                  "FROM users u "
                  "LEFT JOIN departments d ON u.dept_id = d.id";

    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) != SQLITE_OK) {
        SA_LOG_INFO("[DB] Prepare error: %s\n", sqlite3_errmsg(db));
        return users;
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        UserData u;
        u.id = sqlite3_column_int(stmt.get(), 0);
        u.name = (const char*)sqlite3_column_text(stmt.get(), 1);
        u.dept_id = sqlite3_column_int(stmt.get(), 2);
        u.role = sqlite3_column_int(stmt.get(), 3);

        const char* pwd = (const char*)sqlite3_column_text(stmt.get(), 4);
        u.password = pwd ? pwd : "";

        const char* card = (const char*)sqlite3_column_text(stmt.get(), 5);
        u.card_id = card ? card : "";

        const void* blob = sqlite3_column_blob(stmt.get(), 6);
        int bytes = sqlite3_column_bytes(stmt.get(), 6);
        if (bytes > 0 && blob) {
             const uint8_t* ptr = (const uint8_t*)blob;
             u.face_feature.assign(ptr, ptr + bytes);
        }

        // 获取关联查询出来的部门名称 (第7列)
        const char* d_name = (const char*)sqlite3_column_text(stmt.get(), 7);
        u.dept_name = d_name ? d_name : "Unknown";

        const char* avatar = (const char*)sqlite3_column_text(stmt.get(), 8);
        u.avatar_path = avatar ? avatar : "";

        users.push_back(u);
    }

    return users;
}

std::optional<int> db_count_users_by_department(int department_id) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);

    const char* sql = "SELECT COUNT(*) FROM users WHERE dept_id = ?;";
    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int(stmt.get(), 1, department_id);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return sqlite3_column_int(stmt.get(), 0);
}

bool db_assign_user_shift(int user_id, int shift_id) {

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "UPDATE users SET default_shift_id=? WHERE id=?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;

    if (shift_id > 0) sqlite3_bind_int(stmt.get(), 1, shift_id);
    else sqlite3_bind_null(stmt.get(), 1); // 解除排班

    sqlite3_bind_int(stmt.get(), 2, user_id);

    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    return ok;
}

ShiftInfo db_get_user_shift(int user_id) {

    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    ShiftInfo s = {0, "", "", "", "", "", "", "", 0}; // 初始化空结构

    const char* sql =
        "SELECT s.id, s.name, "
        "s.s1_start, s.s1_end, s.s2_start, s.s2_end, s.s3_start, s.s3_end, s.cross_day "
        "FROM users u "
        "JOIN shifts s ON u.default_shift_id = s.id "
        "WHERE u.id = ?;";

    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt.get(), 1, user_id);
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            s.id = sqlite3_column_int(stmt.get(), 0);
            s.name = (const char*)sqlite3_column_text(stmt.get(), 1);

            // 提取所有时段
            auto get_col = [&](int i){ const char* t = (const char*)sqlite3_column_text(stmt.get(), i); return t?t:""; };
            s.s1_start = get_col(2); s.s1_end = get_col(3);
            s.s2_start = get_col(4); s.s2_end = get_col(5);
            s.s3_start = get_col(6); s.s3_end = get_col(7);

            s.cross_day = sqlite3_column_int(stmt.get(), 8);
        }
    }

    return s;
}

//  用户信息修改接口 (不含人脸，密码)
bool db_update_user_basic(int user_id, const std::string& name, int dept_id, int privilege, const std::string& card_id) {

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql =
        "UPDATE users SET name=?, dept_id=?, privilege=?, card_id=? WHERE id=?;";

    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        SA_LOG_ERROR_STREAM() << "[Data] Prepare Update User Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 绑定参数
    sqlite3_bind_text(stmt.get(), 1, name.c_str(), -1, SQLITE_STATIC);

    if (dept_id > 0) sqlite3_bind_int(stmt.get(), 2, dept_id);
    else sqlite3_bind_null(stmt.get(), 2); // 部门为空

    sqlite3_bind_int(stmt.get(), 3, privilege);
    sqlite3_bind_text(stmt.get(), 4, card_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 5, user_id);

    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    if (ok) SA_LOG_INFO_STREAM() << "[Data] User " << user_id << " info updated." << std::endl;
    return ok;
}

//单独更新用户人脸特征数据并更新存储路径(带旧图像清理)
bool db_update_user_face(int user_id, const cv::Mat& face_image) {

    if (face_image.empty()) {
        SA_LOG_ERROR_STREAM() << "[DB] Error: Cannot update face with empty image." << std::endl;
        return false;
    }

    // 1.查找并删除旧的人脸照片
    auto user_opt = db_get_user_info(user_id);
    if (user_opt.has_value()) {
        std::string old_path = user_opt.value().avatar_path;
        // 如果旧路径不为空，且文件真的存在硬盘上
        if (!old_path.empty() && fs::exists(old_path)) {
            try {
                fs::remove(old_path); // 删除旧文件
                SA_LOG_INFO_STREAM() << "[DB] Deleted old avatar file." << std::endl;
            } catch (const fs::filesystem_error& e) {
                SA_LOG_ERROR_STREAM() << "[DB] Warning: Failed to delete old avatar: " << e.what() << std::endl;
            }
        }
    }

    // 开始安全的写操作：加上写锁 (独占锁)
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);
    if (!db) return false;

    // 2. 保存新图片到本地文件夹
    if (!fs::exists(AVATAR_DIR)) {
        fs::create_directories(AVATAR_DIR);
    }

    std::string fname = std::to_string(std::time(nullptr)) + "_" + std::to_string(user_id) + ".jpg";
    fs::path p = fs::path(AVATAR_DIR) / fname;
    std::string path_str = p.string();

    try {
        if (!cv::imwrite(path_str, face_image)) {
            SA_LOG_ERROR_STREAM() << "[DB] Error: Failed to save new face image to disk." << std::endl;
            return false;
        }
    } catch (...) {
        SA_LOG_ERROR_STREAM() << "[DB] Error: Exception occurred while saving image." << std::endl;
        return false;
    }

    // 3. 更新数据库中的 avatar_path
    const char* sql = "UPDATE users SET avatar_path=? WHERE id=?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        SA_LOG_ERROR_STREAM() << "[DB] SQL Error: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt.get(), 1, path_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 2, user_id);

    bool success = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    if (success) {
        SA_LOG_INFO_STREAM() << "[DB] Updated face avatar path." << std::endl;
    }

    return success;
}

// 用户密码更新接口
bool db_update_user_password(int user_id, const std::string& new_raw_password) {

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "UPDATE users SET password=? WHERE id=?;";

    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;

    // 1. 对新密码进行哈希处理 (复用现有的哈希函数)
    std::string hashed_pwd = db_hash_password(new_raw_password);

    // 2. 绑定参数
    sqlite3_bind_text(stmt.get(), 1, hashed_pwd.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 2, user_id);

    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    if (ok) SA_LOG_INFO_STREAM() << "[Data] User " << user_id << " password updated." << std::endl;
    return ok;
}

//单独修改/录入用户指纹特征
bool db_update_user_fingerprint(int user_id, const std::vector<uint8_t>& fingerprint_data) {
    // 写入操作，需要获取排他锁(写锁)
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    // 准备 SQL 更新语句
    const char* sql = "UPDATE users SET fingerprint_data = ? WHERE id = ?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        SA_LOG_ERROR_STREAM() << "[Data] Prepare Update Fingerprint Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 1. 绑定第 1 个参数：fingerprint_data (BLOB类型)
    if (fingerprint_data.empty()) {
        // 如果传入空数组，意味着清空/删除该员工的指纹
        sqlite3_bind_null(stmt.get(), 1);
    } else {
        // 绑定二进制数据：使用 sqlite3_bind_blob
        // 参数解释: (语句句柄, 占位符索引, 数据指针, 数据大小, 内存管理策略)
        sqlite3_bind_blob(stmt.get(), 1, fingerprint_data.data(), fingerprint_data.size(), SQLITE_STATIC);
    }

    // 2. 绑定第 2 个参数：user_id (INTEGER类型)
    sqlite3_bind_int(stmt.get(), 2, user_id);

    // 执行 SQL 语句
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    if (ok) {
        // 检查是否有行被真正修改（防止传入了不存在的 user_id）
        if (sqlite3_changes(db) > 0) {
            SA_LOG_INFO_STREAM() << "[Data] Fingerprint updated successfully for user_id: " << user_id << std::endl;
        } else {
            SA_LOG_ERROR_STREAM() << "[Data] Warning: Fingerprint update affected 0 rows (user_id " << user_id << " might not exist)." << std::endl;
            ok = false;
        }
    } else {
        SA_LOG_ERROR_STREAM() << "[Data] Failed to update fingerprint for user_id: " << user_id
                  << " Error: " << sqlite3_errmsg(db) << std::endl;
    }

    return ok;
}

// 轻量级用户列表加载 (仅 ID 和 Name)
std::vector<UserData> db_get_all_users_light() {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁
    std::vector<UserData> users;

    // SQL 仅查询 id 和 name，绝对不查 face_feature (BLOB)
    const char* sql = "SELECT id, name FROM users;";

    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            UserData u;
            u.id = sqlite3_column_int(stmt.get(), 0);

            const char* txt = (const char*)sqlite3_column_text(stmt.get(), 1);
            u.name = txt ? txt : "Unknown";

            // 其他字段留空即可，因为只是为了映射名字
            users.push_back(u);
        }
    } else {
        SA_LOG_ERROR_STREAM() << "[Data] Light Load Failed: " << sqlite3_errmsg(db) << std::endl;
    }

    // 打印日志方便调试启动速度
    SA_LOG_INFO_STREAM() << "[Data] Light-loaded " << users.size() << " users (ID/Name only)." << std::endl;
    return users;
}

DbUserPageResult db_find_user_basics_page(std::size_t offset, std::size_t limit) {
    using SqliteInteger = sqlite3_int64;
    const auto maxSqliteInteger =
        static_cast<std::size_t>(std::numeric_limits<SqliteInteger>::max());
    if (limit == 0 || limit > kMaxDbUserBasicPageSize ||
        limit >= maxSqliteInteger || offset > maxSqliteInteger) {
        return {DbUserPageStatus::InvalidArgument, {}, false};
    }

    std::shared_lock<std::shared_mutex> lock(g_db_mutex);
    if (db == nullptr) {
        return {DbUserPageStatus::ReadError, {}, false};
    }

    const char* sql =
        "SELECT u.id, u.name, u.dept_id, u.privilege, d.name "
        "FROM users u "
        "LEFT JOIN departments d ON u.dept_id = d.id "
        "ORDER BY u.id LIMIT ? OFFSET ?;";
    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) != SQLITE_OK) {
        SA_LOG_ERROR_STREAM() << "[Data] User Page SQL Error: " << sqlite3_errmsg(db) << std::endl;
        return {DbUserPageStatus::ReadError, {}, false};
    }

    const auto fetchCount = static_cast<SqliteInteger>(limit + 1);
    if (sqlite3_bind_int64(stmt.get(), 1, fetchCount) != SQLITE_OK ||
        sqlite3_bind_int64(stmt.get(), 2, static_cast<SqliteInteger>(offset)) != SQLITE_OK) {
        SA_LOG_ERROR_STREAM() << "[Data] Bind User Page SQL Error: " << sqlite3_errmsg(db) << std::endl;
        return {DbUserPageStatus::ReadError, {}, false};
    }

    try {
        std::vector<UserData> users;
        users.reserve(limit + 1);
        int stepResult = SQLITE_ROW;
        while ((stepResult = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            UserData user;
            user.id = sqlite3_column_int(stmt.get(), 0);
            const char* name = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt.get(), 1));
            user.name = name ? name : "Unknown";
            user.dept_id = sqlite3_column_int(stmt.get(), 2);
            user.role = sqlite3_column_int(stmt.get(), 3);
            const char* departmentName = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt.get(), 4));
            user.dept_name = departmentName ? departmentName : "Unknown";
            users.push_back(std::move(user));
        }

        if (stepResult != SQLITE_DONE) {
            SA_LOG_ERROR_STREAM() << "[Data] Read User Page SQL Error: "
                      << sqlite3_errmsg(db) << std::endl;
            return {DbUserPageStatus::ReadError, {}, false};
        }

        const bool hasMore = users.size() > limit;
        if (hasMore) {
            users.pop_back();
        }
        return {DbUserPageStatus::Success, std::move(users), hasMore};
    } catch (...) {
        return {DbUserPageStatus::ReadError, {}, false};
    }
}



std::vector<UserData> db_get_users_by_dept(int dept_id) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);
    std::vector<UserData> users;

    const char* sql = "SELECT id, name, privilege, dept_id, default_shift_id FROM users WHERE dept_id = ?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        SA_LOG_ERROR_STREAM() << "[Data] Prepare Get Users By Dept Failed: " << sqlite3_errmsg(db) << std::endl;
        return users;
    }

    sqlite3_bind_int(stmt.get(), 1, dept_id);

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        UserData u;
        u.id = sqlite3_column_int(stmt.get(), 0);

        const char* name = (const char*)sqlite3_column_text(stmt.get(), 1);
        u.name = name ? name : "";

        u.role = sqlite3_column_int(stmt.get(), 2);
        u.dept_id = sqlite3_column_int(stmt.get(), 3);
        u.default_shift_id = sqlite3_column_int(stmt.get(), 4);

        users.push_back(u);
    }

    return users;
}
