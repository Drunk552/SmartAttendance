#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_FACE_DATA_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_FACE_DATA_REPOSITORY_H

#include "storage/repository/face_data_repository.h"

namespace smart_attendance::storage::sqlite {

class LegacyFaceDataRepository final : public IFaceDataRepository {
public:
    int cleanupOldAttendanceImages(int daysOld) override;
    std::vector<FaceUserData> listUsersLight() override;
    std::vector<FaceUserData> listUsers() override;
    int addUser(const FaceUserData& user, const cv::Mat& faceImage) override;
    bool updateUserFace(int userId, const cv::Mat& faceImage) override;
    std::vector<FaceAttendanceRecord> records(
        long long startTimestamp, long long endTimestamp) override;
};

} // namespace smart_attendance::storage::sqlite

#endif
