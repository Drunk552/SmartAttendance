#ifndef SMART_ATTENDANCE_CORE_MODEL_SYSTEM_STATS_H
#define SMART_ATTENDANCE_CORE_MODEL_SYSTEM_STATS_H

namespace smart_attendance::core {

struct SystemStats {
    int totalEmployees{0};
    int totalAdmins{0};
    int totalFaces{0};
    int totalFingerprints{0};
    int totalCards{0};
};

} // namespace smart_attendance::core

#endif // SMART_ATTENDANCE_CORE_MODEL_SYSTEM_STATS_H
