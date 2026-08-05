/**
 * @file ui_system_status_mailbox.h
 * @brief 声明后台系统监控到 UI 主线程的有界最新状态邮箱。
 */

#ifndef SMART_ATTENDANCE_APP_UI_SYSTEM_STATUS_MAILBOX_H
#define SMART_ATTENDANCE_APP_UI_SYSTEM_STATUS_MAILBOX_H

#include <cstdint>
#include <mutex>
#include <string>

namespace smart_attendance::app {

struct UiSystemStatusSnapshot {
    std::string timeText;
    std::string weekdayText;
    bool diskFull{false};
    bool diskStatusKnown{false};
    std::uint64_t version{0};
};

/**
 * @brief 保存一个最新系统状态快照，容量固定为一份。
 *
 * Worker 可以覆盖尚未消费的旧时间和磁盘状态；UI 只关心当前状态，不需要重放
 * 已过期的时钟刻度。磁盘状态恢复会覆盖先前告警，消费端最终显示当前真实状态。
 */
class UiSystemStatusMailbox final {
public:
    void publishTime(std::string timeText, std::string weekdayText);
    void publishDiskStatus(bool diskFull);

    /** @brief UI 主线程读取自上次消费后最新的合并状态。 */
    bool tryConsume(UiSystemStatusSnapshot& snapshot);

private:
    std::mutex mutex_;
    UiSystemStatusSnapshot snapshot_;
    std::uint64_t consumedVersion_{0};
};

} // namespace smart_attendance::app

#endif // SMART_ATTENDANCE_APP_UI_SYSTEM_STATUS_MAILBOX_H
