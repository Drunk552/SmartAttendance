#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_FACE_DATA_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_FACE_DATA_REPOSITORY_H

#include "data/db_storage.h"

namespace smart_attendance::storage {

class IFaceDataRepository {
public:
    virtual ~IFaceDataRepository() = default;
    virtual int cleanupOldAttendanceImages(int daysOld) = 0;
    virtual std::vector<UserData> listUsersLight() = 0;
    virtual std::vector<UserData> listUsers() = 0;
    virtual int addUser(const UserData& user, const cv::Mat& faceImage) = 0;
    virtual bool updateUserFace(int userId, const cv::Mat& faceImage) = 0;
    virtual std::vector<AttendanceRecord> records(
        long long startTimestamp, long long endTimestamp) = 0;
};

} // namespace smart_attendance::storage

#endif
