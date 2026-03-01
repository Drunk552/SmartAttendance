#ifndef UI_SCR_RECORD_QUERY_H
#define UI_SCR_RECORD_QUERY_H

#include <lvgl.h>

namespace ui {
namespace record_query {

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