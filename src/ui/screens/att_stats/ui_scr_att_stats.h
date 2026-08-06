#ifndef UI_SCR_ATT_STATS_H
#define UI_SCR_ATT_STATS_H

#include <lvgl.h>

namespace smart_attendance::ui { struct AttendanceStatisticsPageDependencies; }
namespace ui::att_stats {
void configureDependencies(
    smart_attendance::ui::AttendanceStatisticsPageDependencies& dependencies) noexcept;
}

namespace smart_attendance::app {
class UiBackgroundJobQueue;
struct UiBackgroundJobResult;
}

namespace ui {
namespace att_stats {

/** @brief 注入由 Application/TaskManager 持有的 UI 后台任务队列。 */
void configureBackgroundJobs(
    smart_attendance::app::UiBackgroundJobQueue& backgroundJobs) noexcept;

/** @brief UI 关闭前清除任务队列观察指针和页面加载圈记录。 */
void resetBackgroundJobs() noexcept;

/** @brief 由 UI 主循环调用，处理属于考勤统计页面的报表结果。 */
void handleBackgroundJobResult(
    const smart_attendance::app::UiBackgroundJobResult& result);

/**
 * @brief 考勤统计主菜单界面
 * 包含：下载全员报表、下载个人报表入口等
 */
void load_att_stats_menu_screen();

/**
 * @brief 下载 (全员) 考勤报表界面
 */
void load_download_all_screen();

/**
 * @brief 下载 (个人) 考勤报表界面
 */
void load_download_personal_screen();


} // namespace att_stats
} // namespace ui

#endif // UI_SCR_ATT_STATS_H
