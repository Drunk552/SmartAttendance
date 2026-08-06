#ifndef SMART_ATTENDANCE_APP_APPLICATION_ENTRY_H
#define SMART_ATTENDANCE_APP_APPLICATION_ENTRY_H

namespace smart_attendance::app {

/** @brief 创建平台和 Application，运行主循环并返回进程退出码。 */
int runApplication(int argc, char* argv[]);

} // namespace smart_attendance::app

#endif // SMART_ATTENDANCE_APP_APPLICATION_ENTRY_H
