/**
 * @file ui_system_status_mailbox.cpp
 * @brief 实现系统状态的单槽合并邮箱。
 */

#include "ui_system_status_mailbox.h"

#include <utility>

namespace smart_attendance::app {

void UiSystemStatusMailbox::publishTime(std::string timeText,
                                        std::string weekdayText) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.timeText = std::move(timeText);
    snapshot_.weekdayText = std::move(weekdayText);
    ++snapshot_.version;
}

void UiSystemStatusMailbox::publishDiskStatus(bool diskFull) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.diskFull = diskFull;
    snapshot_.diskStatusKnown = true;
    ++snapshot_.version;
}

bool UiSystemStatusMailbox::tryConsume(UiSystemStatusSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.version == consumedVersion_) {
        return false;
    }

    snapshot = snapshot_;
    consumedVersion_ = snapshot_.version;
    return true;
}

} // namespace smart_attendance::app
