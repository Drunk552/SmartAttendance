/**
 * @file repository_error.h
 * @brief 定义存储抽象共用的最小错误类型。
 */

#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_REPOSITORY_ERROR_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_REPOSITORY_ERROR_H

namespace smart_attendance::storage {

enum class RepositoryError {
    ReadFailed,
    WriteFailed
};

} // namespace smart_attendance::storage

#endif // SMART_ATTENDANCE_STORAGE_REPOSITORY_REPOSITORY_ERROR_H
