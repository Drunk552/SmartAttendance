#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_FACE_DATA_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_FACE_DATA_REPOSITORY_H

#include "core/model/legacy_database_models.h"

#include <opencv2/core/mat.hpp>
#include <vector>

namespace smart_attendance::storage {

using FaceUserData = core::LegacyUserData;
using FaceAttendanceRecord = core::LegacyAttendanceRecord;

class IFaceDataRepository {
public:
    virtual ~IFaceDataRepository() = default;
    virtual int cleanupOldAttendanceImages(int daysOld) = 0;
    virtual std::vector<FaceUserData> listUsersLight() = 0;
    virtual std::vector<FaceUserData> listUsers() = 0;
    virtual int addUser(const FaceUserData& user, const cv::Mat& faceImage) = 0;
    virtual bool updateUserFace(int userId, const cv::Mat& faceImage) = 0;
    virtual std::vector<FaceAttendanceRecord> records(
        long long startTimestamp, long long endTimestamp) = 0;
};

} // namespace smart_attendance::storage

#endif
