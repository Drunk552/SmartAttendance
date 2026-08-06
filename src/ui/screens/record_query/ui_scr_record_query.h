#ifndef UI_SCR_RECORD_QUERY_H
#define UI_SCR_RECORD_QUERY_H

#include <lvgl.h>

namespace smart_attendance::ui { struct RecordQueryPageDependencies; }
namespace ui::record_query {
void configureDependencies(
    smart_attendance::ui::RecordQueryPageDependencies& dependencies) noexcept;
}

namespace smart_attendance::app {
class UiBackgroundJobQueue;
struct UiBackgroundJobResult;
}

namespace ui {
namespace record_query {

/** @brief 注入由 Application/TaskManager 拥有的 UI 后台任务队列。 */
void configureBackgroundJobs(
    smart_attendance::app::UiBackgroundJobQueue& backgroundJobs) noexcept;

/** @brief UI 关闭前清除任务队列观察指针和页面加载圈记录。 */
void resetBackgroundJobs() noexcept;

/** @brief 由 UI 主循环调用，处理属于记录查询页面的报表结果。 */
void handleBackgroundJobResult(
    const smart_attendance::app::UiBackgroundJobResult& result);

/**
 * @brief 记录查询主菜单界面
 */
void load_record_query_menu_screen();

/**
 * @brief 工号查询界面
 */
void load_job_query_screen();

/**
 * @brief 浏览工号查询界面
 */
void load_browse_job_query_screen();

/**
 * @brief 下载工号查询界面
 */
void load_download_job_query_screen();

/**
 * @brief 浏览工号查询界面
 */
void load_browse_job_query_result_screen();

/**
 * @brief 时间查询界面
 */
void load_time_query_screen();

/**
 * @brief 浏览时间查询界面
 */
void load_browse_time_query_screen();

/**
 * @brief 浏览时间查询结果界面
 */
void load_browse_time_query_result_screen();

/**
 * @brief 下载时间查询界面
 */
void load_download_time_query_screen();


} // namespace record_query
} // namespace ui

#endif // UI_SCR_RECORD_QUERY_H
