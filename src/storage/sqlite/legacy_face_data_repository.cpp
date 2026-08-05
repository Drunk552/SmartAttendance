#include "legacy_face_data_repository.h"

#include "data/db_storage.h"

namespace smart_attendance::storage::sqlite {

int LegacyFaceDataRepository::cleanupOldAttendanceImages(int daysOld) {
    return db_cleanup_old_attendance_images(daysOld);
}

std::vector<UserData> LegacyFaceDataRepository::listUsersLight() {
    return db_get_all_users_light();
}

std::vector<UserData> LegacyFaceDataRepository::listUsers() {
    return db_get_all_users();
}

int LegacyFaceDataRepository::addUser(const UserData& user,
                                      const cv::Mat& faceImage) {
    return db_add_user(user, faceImage);
}

bool LegacyFaceDataRepository::updateUserFace(int userId,
                                              const cv::Mat& faceImage) {
    return db_update_user_face(userId, faceImage);
}

std::vector<AttendanceRecord> LegacyFaceDataRepository::records(
    long long startTimestamp, long long endTimestamp) {
    return db_get_records(startTimestamp, endTimestamp);
}

} // namespace smart_attendance::storage::sqlite
