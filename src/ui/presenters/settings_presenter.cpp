/**
 * @file settings_presenter.cpp
 * @brief 实现公司名称页面结果映射。
 */

#include "settings_presenter.h"

#include <utility>

namespace smart_attendance::ui {

SettingsPresenter::SettingsPresenter(services::ConfigService& service) noexcept
    : service_(service) {}

bool SettingsPresenter::loadCompanyName(std::string& name) {
    auto result = service_.loadCompanyName();
    if (!result) {
        return false;
    }
    name = std::move(result).value();
    return true;
}

bool SettingsPresenter::saveCompanyName(const std::string& name) {
    return static_cast<bool>(service_.saveCompanyName(name));
}

} // namespace smart_attendance::ui
