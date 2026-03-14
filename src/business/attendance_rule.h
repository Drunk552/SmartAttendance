#ifndef ATTENDANCE_RULE_H
#define ATTENDANCE_RULE_H

#include <string>
#include <ctime>
#include <opencv2/core.hpp> // cv::Mat 用于现场抓拍图

// 打卡状态枚举（考勤规则引擎内部使用）
enum class PunchStatus {
    NORMAL,     // 正常
    LATE,       // 迟到(需记录分钟)
    EARLY,      // 早退(需记录分钟)
    ABSENT      // 旷工
};

// 【新增】打卡结果详情结构体 (Story 1.2)
struct PunchResult {
    PunchStatus status;
    int minutes_diff; // 差异分钟数 (迟到或早退的分钟数，正常为0)
};

struct ShiftConfig{
    std::string start_time; // "09:00"
    std::string end_time;   // "18:00"
int late_threshold_min; // 允许迟到分钟数
};

/**
 * @brief 打卡后的考勤语义结果枚举
 * @details 属于考勤规则域，由 AttendanceRule::recordAttendance 返回，
 *          供 UI 层（ui_controller）根据结果显示对应提示。
 */
enum class RecordResult {
    RECORDED_NORMAL,  // 记录成功，状态：正常
    RECORDED_LATE,    // 记录成功，状态：迟到
    RECORDED_EARLY,   // 记录成功，状态：早退
    RECORDED_ABSENT,  // 记录成功，状态：旷工
    NO_SHIFT,         // 当天无排班（含节点K判定的周末不上班），不写库
    DUPLICATE_PUNCH,  // 防重复打卡，限制时间内重复打卡
    DB_ERROR          // 写入数据库失败
};

class AttendanceRule {
public:

    /**
     * /**
    * @brief 判断打卡归属的班次（处理12:00-13:00 的折中原则）
    * @param punch_timestamp 打卡时间戳
    * @param shift_am 上午班次
    * @param shift_pm 下午班次
    * @return 1: 归属上午, 2: 归属下午
    */
    static int determineShiftOwner(time_t punch_timestamp, const ShiftConfig& shift_am, const ShiftConfig& shift_pm) ;// 判断打卡归属班次

    /**
     * * @brief 计算具体状态（正常、迟到、早退、旷工）
     */
    static PunchResult calculatePunchStatus(time_t punch_timestamp, const ShiftConfig& target_shift, bool is_check_in);// 计算打卡状态

    /**
     * @brief 判断新打卡记录的状态是否比旧记录更“好” (正常优先原则)
     * @param new_status 新的考勤状态
     * @param old_status 之前记录的考勤状态
     * @return true: 新状态更优 (应该覆盖), false: 新状态不如旧状态
     */
    static bool isStatusBetter(int new_status, int old_status);

    // 辅助函数：将 "HH:MM" 转换为当天的第 N 分钟 (Task Book 4.2)
    static int timeStringToMinutes(const std::string& time_str);

    /**
     * @brief 核心考勤记录函数（验证成功后由 UI 层调用）
     *
     * 完整实现打卡考勤计算流程，严格对应流程图逻辑：
     *   1. 调用 db_get_user_shift_smart 获取当天排班（含节点K周末规则）
     *   2. 无排班 → 返回 NO_SHIFT，不写入数据库
     *   3. 防重复打卡检查
     *   4. 折中原则判断打卡归属（上班/下班）
     *   5. 计算打卡状态（正常/迟到/早退/旷工）
     *   6. 写入数据库
     *
     * @param user_id 已验证成功的用户ID
     * @param image   现场抓拍图（可为空 cv::Mat）
     * @return RecordResult 考勤语义结果
     */
    static RecordResult recordAttendance(int user_id, const cv::Mat& image = cv::Mat());

};

#endif // ATTENDANCE_RULE_H
