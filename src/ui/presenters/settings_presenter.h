/**
 * @file settings_presenter.h
 * @brief 声明系统设置页面的轻量 Presenter。
 */

#ifndef SMART_ATTENDANCE_UI_PRESENTERS_SETTINGS_PRESENTER_H
#define SMART_ATTENDANCE_UI_PRESENTERS_SETTINGS_PRESENTER_H

#include "services/config_service.h"

#include <string>

namespace smart_attendance::ui {

/**
 * @brief 将公司名称配置结果映射为旧页面使用的布尔返回语义。
 * @note 不持有 LVGL 控件或 Repository；方法应由 UI 主线程调用。
 */
class SettingsPresenter final {
public:
    explicit SettingsPresenter(services::ConfigService& service) noexcept;

    bool loadCompanyName(std::string& name);
    bool saveCompanyName(const std::string& name);

private:
    services::ConfigService& service_;
};

} // namespace smart_attendance::ui

#endif // SMART_ATTENDANCE_UI_PRESENTERS_SETTINGS_PRESENTER_H
