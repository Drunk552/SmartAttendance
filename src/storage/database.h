/**
 * @file database.h
 * @brief 声明当前 SQLite 数据库的受控生命周期入口。
 *
 * 自由函数保留既有 ABI，供 Application 的函数指针生命周期适配器使用。
 * Repository 不负责打开或关闭数据库。
 */

#ifndef SMART_ATTENDANCE_STORAGE_DATABASE_H
#define SMART_ATTENDANCE_STORAGE_DATABASE_H

#include <string>

bool data_init();
bool data_is_open();
bool data_seed();
void data_close();
std::string db_hash_password(const std::string& rawPassword);

#endif // SMART_ATTENDANCE_STORAGE_DATABASE_H
