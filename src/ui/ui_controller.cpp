/**
 * @file ui_controller.cpp
 * @brief UI 控制器实现文件 - 提供 UI 层与业务/数据层的接口封装
 * @details 该类封装了 UI 层所需的各种业务逻辑调用，简化 UI 代码复杂度。
 *          通过单例模式提供全局访问点。
 */
#include "ui_controller.h"
// 引入原来 ui_app.cpp 依赖的底层头文件
#include "../business/face_demo.h"
#include "../business/attendance_rule.h"
#include "../app/ui_system_status_mailbox.h"
#include "hal/rtc.h"
#include "hal/storage_device.h"
#include "storage/repository/employee_settings_import_repository.h"
#include "presenters/employee_lookup_presenter.h"
#include "presenters/settings_presenter.h"
#include "presenters/department_presenter.h"
#include "presenters/system_info_presenter.h"
#include "managers/ui_manager.h"
#include <algorithm>
#include <set>
#include <cstdio>
#include <filesystem>
#include <thread> // for sleep if needed
#include <cstring> // memcpy
#include <sstream>
#include <fstream>
#include <regex>
#include <iostream> 
#include <chrono>
#include <condition_variable>
#include <utility>

namespace fs = std::filesystem;// C++17 引入的文件系统库


namespace {
std::mutex g_monitorWaitMutex;
std::condition_variable g_monitorWaitCondition;
smart_attendance::hal::IRtc* g_rtc = nullptr;
smart_attendance::hal::IStorageDevice* g_storage = nullptr;

std::time_t currentUnixTime() {
    if (g_rtc != nullptr) {
        const auto result = g_rtc->now();
        if (result) {
            return static_cast<std::time_t>(result.value().unixSeconds);
        }
    }
    return 0;
}

std::filesystem::path storageDirectory(
    const std::filesystem::path& relativePath) {
    if (g_storage != nullptr) {
        auto result = g_storage->ensureDirectory(relativePath);
        if (result) {
            return result.value();
        }
    }
    return {};
}

bool isDiskFull() {
    if (g_storage != nullptr) {
        const auto root = g_storage->ensureDirectory(".");
        if (!root) {
            return false;
        }
        const auto storageSpace = g_storage->space();
        if (storageSpace) {
            return storageSpace.value().availableBytes <
                   100ULL * 1024ULL * 1024ULL;
        }
    }
    return false;
}

std::string currentTimeString() {
    std::time_t rawtime = currentUnixTime();
    std::tm timeinfo{};
    localtime_r(&rawtime, &timeinfo);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    return std::string(buf);
}

std::string currentWeekdayString() {
    const std::time_t now = currentUnixTime();
    std::tm timeinfo{};
    localtime_r(&now, &timeinfo);
    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%a", &timeinfo);
    return buffer;
}

} // namespace

void uiConfigureDeviceServices(
    smart_attendance::hal::IRtc& rtc,
    smart_attendance::hal::IStorageDevice& storage) noexcept {
    g_rtc = &rtc;
    g_storage = &storage;
}

void uiResetDeviceServices() noexcept {
    g_rtc = nullptr;
    g_storage = nullptr;
}

// 移入原 check_disk_low 逻辑
bool UiController::isDiskFull() {
    return ::isDiskFull();
}

std::time_t UiController::getCurrentUnixTime() {
    return currentUnixTime();
}

// 移入原 get_current_time_str 逻辑
std::string UiController::getCurrentTimeStr() {
    return currentTimeString();
}

// 获取当前星期几字符串实现
std::string UiController::getCurrentWeekdayStr() {
    return currentWeekdayString();
}

// ==================== 公司设置功能实现  ====================

/**
 * @brief 保存公司名称到数据库
 * @param name 公司名称
 * @return true 保存成功；false 保存失败
 */
bool UiController::saveCompanyName(const std::string& name) {
    if (settingsPresenter_ != nullptr) {
        return settingsPresenter_->saveCompanyName(name);
    }

    return false;
}

/**
 * @brief 从数据库加载公司名称
 * @param name 输出：公司名称
 * @return true 加载成功；false 加载失败
 */
bool UiController::loadCompanyName(std::string& name) {
    if (settingsPresenter_ != nullptr) {
        return settingsPresenter_->loadCompanyName(name);
    }

    return false;
}

// 移入原 get_next_available_id 逻辑
int UiController::generateNextUserId() {
    if (employeeLookupPresenter_ == nullptr) return 0;
    const auto users = employeeLookupPresenter_->listAll();
    int max_id = 0;
    
    // 遍历所有用户，找出目前最大的 ID
    for (const auto& user : users) {
        if (user.id > max_id) {
            max_id = user.id;
        }
    }
    // 如果没有任何用户(max_id=0)，默认从 1 开始
    // 否则，返回 最大值 + 1
    if (max_id == 0) {
        return 1; 
    } else {
        return max_id + 1;
    }
}

std::vector<DeptInfo> UiController::getDepartmentList() {
    if (departmentPresenter_ != nullptr) {
        std::vector<smart_attendance::ui::DepartmentItem> items;
        if (!departmentPresenter_->listDepartments(items)) {
            return {};
        }
        std::vector<DeptInfo> departments;
        departments.reserve(items.size());
        for (const auto& item : items) {
            DeptInfo department;
            department.id = item.id;
            department.name = item.name;
            department.company_id = item.companyId;
            departments.push_back(std::move(department));
        }
        return departments;
    }
    return {};
}

// 通过部门 ID 获取部门名称的实现
std::string UiController::getDeptNameById(int deptId) {
    // 1. 获取所有部门列表
    std::vector<DeptInfo> depts = getDepartmentList();
    
    // 2. 遍历查找匹配的 ID
    for (const auto& dept : depts) {
        if (dept.id == deptId) {
            return dept.name; // 找到了，返回真实部门名称
        }
    }
    return "未知部门"; // 没找到时的默认返回值
}

// ==================== 部门管理功能实现  ====================

bool UiController::addDepartment(const std::string& deptName) {
    if (departmentPresenter_ != nullptr) {
        return departmentPresenter_->addDepartment(deptName);
    }

    return false;
}

bool UiController::updateDepartment(int deptId, const std::string& newName) {
    if (departmentPresenter_ != nullptr) {
        return departmentPresenter_->renameDepartment(deptId, newName);
    }

    return false;
}

bool UiController::deleteDepartment(int deptId) {
    if (departmentPresenter_ != nullptr) {
        return departmentPresenter_->removeDepartment(deptId);
    }

    return false;
}

int UiController::getDepartmentEmployeeCount(int deptId) {
    if (departmentPresenter_ != nullptr) {
        int count = 0;
        return departmentPresenter_->employeeCount(deptId, count) ? count : 0;
    }

    return 0;
}

// 获取指定部门的排班视图
DeptScheduleView UiController::getDeptSchedule(int deptId) {
    if (departmentPresenter_ != nullptr) {
        smart_attendance::ui::DepartmentScheduleState state;
        if (!departmentPresenter_->loadSchedule(deptId, state)) {
            DeptScheduleView empty;
            empty.dept_id = deptId;
            for (int& shift : empty.shifts) {
                shift = 0;
            }
            return empty;
        }

        DeptScheduleView view;
        view.dept_id = state.departmentId;
        view.dept_name = state.departmentName;
        for (std::size_t index = 0; index < state.shiftIds.size(); ++index) {
            view.shifts[index] = state.shiftIds[index];
        }
        return view;
    }
    return {};
}

// 更新指定部门的名称和排班信息
bool UiController::updateDeptSchedule(int deptId, const std::string& newName, const std::vector<int>& shifts) {
    if (departmentPresenter_ != nullptr) {
        return departmentPresenter_->updateSchedule(deptId, newName, shifts);
    }

    return false;
}

bool UiController::registerNewUser(const std::string& name, int deptId) {
    // 调用业务层接口
    return business_register_user(name.c_str(), deptId);
}

void UiController::configureEmployeeLookupPresenter(
    smart_attendance::ui::EmployeeLookupPresenter* presenter) noexcept {
    employeeLookupPresenter_ = presenter;
}

void UiController::configureSettingsPresenter(
    smart_attendance::ui::SettingsPresenter* presenter) noexcept {
    settingsPresenter_ = presenter;
}

void UiController::configureDepartmentPresenter(
    smart_attendance::ui::DepartmentPresenter* presenter) noexcept {
    departmentPresenter_ = presenter;
}

void UiController::configureShiftPresenter(
    smart_attendance::ui::ShiftPresenter* presenter) noexcept {
    shiftPresenter_ = presenter;
}

void UiController::configureAttendanceQueryPresenter(
    smart_attendance::ui::AttendanceQueryPresenter* presenter) noexcept {
    attendanceQueryPresenter_ = presenter;
}

void UiController::configureMaintenancePresenter(
    smart_attendance::ui::MaintenancePresenter* presenter) noexcept {
    maintenancePresenter_ = presenter;
}

void UiController::configureSystemInfoPresenter(
    smart_attendance::ui::SystemInfoPresenter* presenter) noexcept {
    systemInfoPresenter_ = presenter;
}

int UiController::getUserRoleById(int userId) {
    if (employeeLookupPresenter_ == nullptr) {
        return -1;
    }
    return employeeLookupPresenter_->roleValueById(userId);
}

// 验证用户密码是否正确（哈希验证）
bool UiController::verifyUserPassword(int userId, const std::string& inputPassword) {
    if (employeeLookupPresenter_ != nullptr) {
        return employeeLookupPresenter_->verifyPassword(userId, inputPassword);
    }

    return false;
}

std::vector<UserData> UiController::getAllUsers() {
    if (employeeLookupPresenter_ == nullptr) return {};

    const auto items = employeeLookupPresenter_->listAll();
    std::vector<UserData> users;
    users.reserve(items.size());
    for (const auto& item : items) {
        UserData user;
        user.id = item.id;
        user.name = item.name;
        user.dept_id = item.departmentId;
        user.dept_name = item.departmentName;
        user.role = item.role;
        users.push_back(std::move(user));
    }
    return users;
}

UserDisplayInfo UiController::getUserDisplayInfo(int userId) {
    if (employeeLookupPresenter_ == nullptr) return UserDisplayInfo{};

    const auto item = employeeLookupPresenter_->findDisplayDetailsById(userId);
    if (!item) {
        return UserDisplayInfo{};
    }

    UserDisplayInfo display;
    display.id = item->id;
    display.name = item->name;
    display.departmentId = item->departmentId;
    display.departmentName = item->departmentName;
    display.faceRegistered = item->faceRegistered;
    display.fingerprintRegistered = item->fingerprintRegistered;
    display.cardId = item->cardId;
    display.passwordRegistered = item->passwordRegistered;
    display.role = item->role;
    return display;
}

//获取班次列表实现
std::vector<ShiftInfo> UiController::getAllShifts() {
    if (shiftPresenter_ != nullptr) {
        const auto items = shiftPresenter_->listAll();
        std::vector<ShiftInfo> shifts;
        shifts.reserve(items.size());
        for (const auto& item : items) {
            shifts.push_back({item.id, item.name, item.firstStart, item.firstEnd,
                              item.secondStart, item.secondEnd, item.thirdStart,
                              item.thirdEnd, item.crossDay});
        }
        return shifts;
    }
    return {};
}

//获取指定班次的详细信息实现
std::optional<ShiftInfo> UiController::getShiftInfo(int shiftId) {
    if (shiftPresenter_ != nullptr) {
        smart_attendance::ui::ShiftItem item;
        if (!shiftPresenter_->findById(shiftId, item)) return std::nullopt;
        return ShiftInfo{item.id, item.name, item.firstStart, item.firstEnd,
                         item.secondStart, item.secondEnd, item.thirdStart,
                         item.thirdEnd, item.crossDay};
    }
    return std::nullopt;
}

//更新班次信息实现
bool UiController::updateShiftInfo(int shift_id, 
                                   const std::string& s1_start, const std::string& s1_end,
                                   const std::string& s2_start, const std::string& s2_end,
                                   const std::string& s3_start, const std::string& s3_end) {
    if (shiftPresenter_ != nullptr) {
        smart_attendance::ui::ShiftItem current;
        if (!shiftPresenter_->findById(shift_id, current)) return false;
        current.firstStart = s1_start;
        current.firstEnd = s1_end;
        current.secondStart = s2_start;
        current.secondEnd = s2_end;
        current.thirdStart = s3_start;
        current.thirdEnd = s3_end;
        current.crossDay = 0;
        return shiftPresenter_->update(current);
    }
    return false;
}

int UiController::getUserCount() {
    return business_get_user_count();
}

bool UiController::getUserAt(int index, int* id, char* name_buf, int buf_len) {
    return business_get_user_at(index, id, name_buf, buf_len);
}

UserData UiController::getUserInfo(int uid) {
    UserData empty_user;
    empty_user.id = 0;
    //设一个默认名字防止界面显示乱码
    empty_user.name = "Unknown"; 
    
    return empty_user;
}

// 检查用户是否存在 (用于 UI 导出报表前的同步校验)
bool UiController::checkUserExists(int user_id) {
    return employeeLookupPresenter_ != nullptr &&
           employeeLookupPresenter_->existsById(user_id);
}

std::vector<AttendanceRecord> UiController::getRecords(int userId, time_t start, time_t end) {
    if (attendanceQueryPresenter_ != nullptr) {
        const auto items = attendanceQueryPresenter_->query(userId, start, end);
        std::vector<AttendanceRecord> records;
        records.reserve(items.size());
        for (const auto& item : items) {
            AttendanceRecord record{};
            record.id = item.id;
            record.user_id = item.employeeId;
            record.user_name = item.employeeName;
            record.dept_name = item.departmentName;
            record.timestamp = item.timestamp;
            record.status = item.status;
            record.image_path = item.imagePath;
            records.push_back(std::move(record));
        }
        return records;
    }
    return {};
}

smart_attendance::ui::AttendanceRecordPage UiController::getRecordPage(
    int userId, time_t start, time_t end, std::size_t pageIndex) {
    if (attendanceQueryPresenter_ == nullptr) return {};
    return attendanceQueryPresenter_->queryPage(userId, start, end, pageIndex);
}

bool UiController::getDisplayFrame(uint8_t* buffer, int width, int height) {
    // 1. 加锁
    std::lock_guard<std::mutex> lock(m_frame_mutex);
    
    // 2. 检查是否有数据
    if (m_cached_frame.empty()) {
        return false; // 还没采集到第一帧
    }
    
    // 3. 检查缓冲区大小是否匹配 (防止越界)
    size_t required_size = width * height * 3;
    if (m_cached_frame.size() < required_size) {
        return false;
    }

    // 4. [快速操作] 仅拷贝内存，不进行任何硬件IO
    std::memcpy(buffer, m_cached_frame.data(), required_size);
    
    return true;
}

// 更新用户名称实现
bool UiController::updateUserName(int userId, const std::string& newName) {
    if (employeeLookupPresenter_ != nullptr) {
        return employeeLookupPresenter_->updateName(userId, newName);
    }

    return false;
}

//更新用户部门信息
bool UiController::updateUserDept(int userId, int newDeptId) {
    if (employeeLookupPresenter_ != nullptr) {
        return employeeLookupPresenter_->updateDepartment(userId, newDeptId);
    }

    return false;
}

//更新用户人脸
bool UiController::updateUserFace(int userId) {
    // 直接调用业务层刚刚封装好的接口！
    // 业务层会自动抓取当前画面、更新数据库，并刷新识别模型。
    return business_update_user_face(userId);
}

// 更新用户密码实现
bool UiController::updateUserPassword(int userId, const std::string& newPassword) {
    if (employeeLookupPresenter_ != nullptr) {
        return employeeLookupPresenter_->updatePassword(userId, newPassword);
    }

    return false;
}

// 更新用户权限实现
bool UiController::updateUserRole(int userId, int newRole) {
    if (employeeLookupPresenter_ != nullptr) {
        return employeeLookupPresenter_->updateRole(userId, newRole);
    }

    return false;
}

// 删除用户实现
bool UiController::deleteUser(int userId) {
    if (employeeLookupPresenter_ != nullptr) {
        return employeeLookupPresenter_->remove(userId);
    }

    return false;
}

// 更新摄像头 Buffer 实现
void UiController::updateCameraFrame(const uint8_t* data, int w, int h) {
    // 1. 获取 UI 层的显示 Buffer
    uint8_t* disp_buf = UiManager::getInstance()->getCameraDisplayBuffer();
    
    // 2. 只有当 Buffer 有效且尺寸匹配时才拷贝
    // 假设 CAM_W 和 CAM_H 是 240x320
    if (disp_buf && data) {
        // 计算数据量: 宽 * 高 * 3字节(RGB)
        size_t size = w * h * 3;
        memcpy(disp_buf, data, size);
    }
}

//查询系统信息
SystemStats UiController::getSystemStatistics() {
    SystemStats stats{};
    return systemInfoPresenter_ != nullptr && systemInfoPresenter_->statistics(stats)
        ? stats
        : SystemStats{};
}

bool UiController::clearAllRecords() {
    if (maintenancePresenter_ != nullptr) {
        return maintenancePresenter_->clearAttendance();
    }
    return false;
}

// 恢复出厂设置实现
bool UiController::factoryReset() {
    if (maintenancePresenter_ != nullptr) {
        return maintenancePresenter_->factoryReset();
    }
    return false;
}

// 清除所有员工实现 (防止下一个报错是它)
bool UiController::clearAllEmployees() {
    if (maintenancePresenter_ != nullptr) {
        return maintenancePresenter_->clearEmployees();
    }
    return false;
}

// 清除所有数据实现
bool UiController::clearAllData() {
    if (maintenancePresenter_ != nullptr) {
        return maintenancePresenter_->clearAllData();
    }
    return false;
}

void uiRunMonitorTask(
    const std::atomic<bool>& stopRequested,
    smart_attendance::app::UiSystemStatusMailbox& statusMailbox) {
    int diskCheckCounter = 0;
    while (!stopRequested.load()) {
        // 单槽邮箱只保留最新状态；后台线程不接触 LVGL。
        statusMailbox.publishTime(
            currentTimeString(),
            currentWeekdayString());

        // 磁盘状态同样按当前值合并，避免积压过期告警。
        if (++diskCheckCounter >= 5) {
            diskCheckCounter = 0;
            statusMailbox.publishDiskStatus(::isDiskFull());
        }

        std::unique_lock<std::mutex> lock(g_monitorWaitMutex);
        g_monitorWaitCondition.wait_for(
            lock,
            std::chrono::seconds(1),
            [&stopRequested]() { return stopRequested.load(); });
    }
}

void uiWakeMonitorTask() {
    g_monitorWaitCondition.notify_all();
}

// ============================================================
// 上传员工设置表实现
// 流程：从当前 IStorageDevice 的 usb_settings/员工设置表.xlsx 解压导入
//       → 读取 sharedStrings.xml 建立字符串索引
//       → 读取 sheet1.xml 第6行起解析员工数据(工号/姓名/部门/权限)
//       → 调用 db_batch_add_users 写入数据库
// ============================================================
bool uiImportEmployeeSettings(
    smart_attendance::storage::IEmployeeSettingsImportRepository& repository,
    int* invalid_time_count) {
    const auto settingsDirectory =
        storageDirectory("usb_settings");
    if (settingsDirectory.empty()) return false;
    const std::string xlsx_path =
        (settingsDirectory / "员工设置表.xlsx").string();
    const std::string tmp_dir   = "output/.xlsx_import_tmp";

    // 1. 检查文件是否存在
    if (!fs::exists(xlsx_path)) {
        printf("[Import] 员工设置表不存在: %s\n", xlsx_path.c_str());
        return false;
    }

    // 2. 清理并创建临时解压目录
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
    fs::create_directories(tmp_dir, ec);

    // 3. 调用 unzip 解压 xlsx
    std::string cmd = "unzip -o \"" + xlsx_path + "\" -d \"" + tmp_dir + "\" > /dev/null 2>&1";
    if (std::system(cmd.c_str()) != 0) {
        printf("[Import] 解压 xlsx 失败\n");
        return false;
    }

    // ---- 辅助 Lambda：读取整个文件内容 ----
    auto read_file_content = [](const std::string& path) -> std::string {
        std::ifstream f(path, std::ios::in | std::ios::binary);
        if (!f.is_open()) return "";
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    };

    // 4. 解析 sharedStrings.xml，建立 index → string 映射
    std::vector<std::string> shared_strings;
    {
        std::string ss_xml = read_file_content(tmp_dir + "/xl/sharedStrings.xml");
        if (ss_xml.empty()) {
            printf("[Import] 无法读取 sharedStrings.xml\n");
            return false;
        }
        // 匹配每一个 <si><t>...</t></si>（也兼容带属性的 <t xml:space="preserve">）
        std::regex si_re("<si>[\\s\\S]*?<t[^>]*>([^<]*)</t>[\\s\\S]*?</si>");
        auto begin = std::sregex_iterator(ss_xml.begin(), ss_xml.end(), si_re);
        auto end   = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            shared_strings.push_back((*it)[1].str());
        }
        printf("[Import] sharedStrings 共 %zu 条\n", shared_strings.size());
    }

    // 辅助 Lambda：根据单元格类型和值取实际字符串
    auto get_cell_str = [&](const std::string& type, const std::string& val) -> std::string {
        if (type == "s") {
            // 字符串引用：val 是 sharedStrings 的 index
            int idx = std::stoi(val);
            if (idx >= 0 && idx < (int)shared_strings.size()) {
                return shared_strings[idx];
            }
            return "";
        }
        return val; // 数字或其他直接返回
    };

    // 5. 解析 sheet1.xml，提取第6行起的员工数据
    // 表格列定义（1-indexed 对应 A~D）：
    //   A(col 1) = 工号(数字)
    //   B(col 2) = 姓名(字符串)
    //   C(col 3) = 部门ID(数字)
    //   D(col 4) = 权限(数字, 0=普通 1=管理员)
    std::string sheet_xml = read_file_content(tmp_dir + "/xl/worksheets/sheet1.xml");
    if (sheet_xml.empty()) {
        printf("[Import] 无法读取 sheet1.xml\n");
        return false;
    }

    // 解析列号字母 → 数字（A=1, B=2, ..., Z=26, AA=27 ...）
    auto col_letter_to_num = [](const std::string& col) -> int {
        int num = 0;
        for (char c : col) {
            num = num * 26 + (c - 'A' + 1);
        }
        return num;
    };

    std::vector<UserData> import_users;

    // 用于保存提取到的年份和月份，兜底使用当前年月
    int import_year = 0, import_month = 0;
    time_t now = currentUnixTime();
    tm *ltm = localtime(&now);
    import_year = 1900 + ltm->tm_year;
    import_month = 1 + ltm->tm_mon;

    // 逐行解析：匹配 <row r="N" ...>...</row>
    std::regex row_re("<row r=\"(\\d+)\"[^>]*>([\\s\\S]*?)</row>");
    // 匹配行内每个单元格：<c r="A6" t="s"><v>3</v></c>
    std::regex cell_re("<c r=\"([A-Z]+)(\\d+)\"(?:[^>]* t=\"([^\"]*)\")?[^>]*>(?:<v>([^<]*)</v>)?");

    auto row_begin = std::sregex_iterator(sheet_xml.begin(), sheet_xml.end(), row_re);
    auto row_end   = std::sregex_iterator();

    for (auto row_it = row_begin; row_it != row_end; ++row_it) {
        int row_num = std::stoi((*row_it)[1].str());
        std::string row_content = (*row_it)[2].str();

        // 提取该行各列的值
        std::map<int, std::string> col_vals; // col_num -> 实际字符串值
        auto cell_begin = std::sregex_iterator(row_content.begin(), row_content.end(), cell_re);
        auto cell_end   = std::sregex_iterator();

        for (auto cell_it = cell_begin; cell_it != cell_end; ++cell_it) {
            std::string col_letters = (*cell_it)[1].str();
            std::string type        = (*cell_it)[3].str(); // "s" 或 ""
            std::string val         = (*cell_it)[4].str(); // <v> 中的值

            if (val.empty()) continue; // 空单元格跳过

            int col_num = col_letter_to_num(col_letters);
            col_vals[col_num] = get_cell_str(type, val);
        }

        // 解析第1行的表头标题，提取年份和月份
        if (row_num == 1) {
            if (col_vals.count(1)) {
                std::string title = col_vals[1];
                std::regex title_regex(R"((\d{4})\s*年\s*(\d{1,2})\s*月)");
                std::smatch match;
                if (std::regex_search(title, match, title_regex)) {
                    import_year = std::stoi(match[1]);
                    import_month = std::stoi(match[2]);
                    printf("[Import] 成功提取到排班表年月: %d-%02d\n", import_year, import_month);
                }
            }
            continue; // 第1行处理完直接跳过
        }

        // 第2~5行是普通的表头，直接跳过
        if (row_num < 6) continue; 

        // === 下面是第6行起的数据行 ===
        // A(1)=工号, B(2)=姓名, C(3)=部门ID, D(4)=权限
        if (col_vals.find(1) == col_vals.end() || col_vals.find(2) == col_vals.end()) {
            continue;
        }

        int user_id = 0;
        try { user_id = std::stoi(col_vals[1]); } catch (...) { continue; }
        if (user_id <= 0) continue;

        UserData u;
        u.id              = user_id;
        u.name            = col_vals[2];
        u.dept_id         = (col_vals.count(3) && !col_vals[3].empty()) ? std::stoi(col_vals[3]) : 0;
        u.role            = (col_vals.count(4) && !col_vals[4].empty()) ? std::stoi(col_vals[4]) : 0;
        u.default_shift_id = 0;
        u.card_id         = "";
        u.password        = "";

        // 从第5列(E)开始，提取 1~31 天的排班
        for (int day = 1; day <= 31; ++day) {
            int col_index = day + 4; // 1号对应第5列，2号对应第6列...
            int shift_id = 0;
            if (col_vals.count(col_index) && !col_vals[col_index].empty()) {
                std::string shift_str = col_vals[col_index];
                if (shift_str != "-" && shift_str != "休") {
                    try { shift_id = std::stoi(shift_str); } catch (...) { shift_id = 0; }
                }
            }
            u.monthly_schedule[day] = shift_id; // 存入该员工的字典
        }

        import_users.push_back(u);
        printf("[Import] 读取员工: id=%d name=%s dept=%d role=%d (已读取排班)\n",
               u.id, u.name.c_str(), u.dept_id, u.role);
    }

    // ============================================================
    // 6. 解析 sheet2.xml（考勤设置表）- 班次时间校验与写入
    // 表格列映射（第5~14行对应班次1~10，1-indexed列）：
    //   A(1)=班次号  B(2)=s1_start  C(3)=s1_end
    //   D(4)=s2_start  E(5)=s2_end  F(6)=s3_start  G(7)=s3_end
    // ============================================================
    int bad_time_count = 0; // 记录时间格式非法的字段数

    std::string sheet2_xml = read_file_content(tmp_dir + "/xl/worksheets/sheet2.xml");
    if (!sheet2_xml.empty()) {
        // 时间校验辅助 Lambda：调用 AttendanceRule::timeStringToMinutes
        // 返回规范化的 "HH:MM" 字符串；非法时返回 "" 并计数
        auto validate_time = [&](const std::string& raw) -> std::string {
            if (raw.empty()) return ""; // 空格=未设置，直接透传空串
            
            std::string time_to_parse = raw;
            
            // 拦截并转换 Excel 浮点数时间格式 (如 "0.33333333333333" 代表 08:00)
            if (raw.find('.') != std::string::npos) {
                try {
                    double time_fraction = std::stod(raw);
                    // 确保是一个合理的时间比例值 (0.0 到 1.0 之间)
                    if (time_fraction >= 0.0 && time_fraction < 1.0) {
                        // 换算成当天的总分钟数
                        int total_mins = std::round(time_fraction * 24 * 60);
                        char buf[8];
                        snprintf(buf, sizeof(buf), "%02d:%02d", total_mins / 60, total_mins % 60);
                        time_to_parse = std::string(buf);
                    }
                } catch (...) {
                    // 如果转换出错 (例如并非真正的数字)，则静默忽略，将原字符串交给业务层报错
                }
            }

            // 使用转换后(或原本就合法)的字符串去获取分钟数
            int mins = AttendanceRule::timeStringToMinutes(time_to_parse);
            
            if (mins < 0) {
                // 非法格式：记录日志并计数，返回空串（写DB时跳过）
                printf("[Import] 时间格式非法，已跳过: \"%s\" (原始值:%s)\n", time_to_parse.c_str(), raw.c_str());
                ++bad_time_count;
                return "";
            }
            
            // 合法：格式化为标准 "HH:MM" 写回
            char buf[8];
            snprintf(buf, sizeof(buf), "%02d:%02d", mins / 60, mins % 60);
            return std::string(buf);
        };

        // 逐行扫描 sheet2，第5行(r=5)到第14行(r=14)对应班次1~10
        auto s2_row_begin = std::sregex_iterator(sheet2_xml.begin(), sheet2_xml.end(), row_re);
        auto s2_row_end   = std::sregex_iterator();

        for (auto row_it = s2_row_begin; row_it != s2_row_end; ++row_it) {
            int row_num = std::stoi((*row_it)[1].str());
            if (row_num < 5 || row_num > 14) continue; // 只处理班次行

            std::string row_content = (*row_it)[2].str();

            // 提取该行各列原始值
            std::map<int, std::string> s2_cols;
            auto cell_begin = std::sregex_iterator(row_content.begin(), row_content.end(), cell_re);
            auto cell_end   = std::sregex_iterator();
            for (auto cell_it = cell_begin; cell_it != cell_end; ++cell_it) {
                std::string col_letters = (*cell_it)[1].str();
                std::string type        = (*cell_it)[3].str();
                std::string val         = (*cell_it)[4].str();
                if (val.empty()) continue;
                int col_num = col_letter_to_num(col_letters);
                s2_cols[col_num] = get_cell_str(type, val);
            }

            // A列(1)=班次号，必须是1~10
            if (s2_cols.find(1) == s2_cols.end()) continue;
            int shift_id = 0;
            try { shift_id = std::stoi(s2_cols[1]); } catch (...) { continue; }
            if (shift_id < 1 || shift_id > 10) continue;

            // 读取并校验6个时间字段（空字段视为"未设置"，保留空串）
            std::string s1_start = validate_time(s2_cols.count(2) ? s2_cols[2] : "");
            std::string s1_end   = validate_time(s2_cols.count(3) ? s2_cols[3] : "");
            std::string s2_start = validate_time(s2_cols.count(4) ? s2_cols[4] : "");
            std::string s2_end   = validate_time(s2_cols.count(5) ? s2_cols[5] : "");
            std::string s3_start = validate_time(s2_cols.count(6) ? s2_cols[6] : "");
            std::string s3_end   = validate_time(s2_cols.count(7) ? s2_cols[7] : "");

            printf("[Import] 班次%d: s1[%s-%s] s2[%s-%s] s3[%s-%s]\n",
                   shift_id,
                   s1_start.empty() ? "--" : s1_start.c_str(),
                   s1_end.empty()   ? "--" : s1_end.c_str(),
                   s2_start.empty() ? "--" : s2_start.c_str(),
                   s2_end.empty()   ? "--" : s2_end.c_str(),
                   s3_start.empty() ? "--" : s3_start.c_str(),
                   s3_end.empty()   ? "--" : s3_end.c_str());

            // 写入数据库（只更新有效时间字段，cross_day 保持原值0）
            (void)repository.updateShift(
                shift_id, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end);
        }
    } else {
        printf("[Import] sheet2.xml 不存在或为空，跳过班次时间导入\n");
    }

    // 向调用方汇报非法时间字段数
    if (invalid_time_count) *invalid_time_count = bad_time_count;

    // 7. 清理临时目录
    fs::remove_all(tmp_dir, ec);

    if (import_users.empty()) {
        printf("[Import] 未读取到任何有效员工数据\n");
        // 即使没有员工数据，只要班次时间写入成功也视为部分成功
        // 这里返回 false 让调用方知道员工表为空，但班次已写入
        return false;
    }

    // 8. 员工数据写入数据库（INSERT OR REPLACE）
    bool ok = repository.importUsers(import_year, import_month, import_users);

    printf("[Import] 导入结果: %s,共 %zu 名员工，时间格式异常字段: %d\n",
           ok ? "成功" : "失败", import_users.size(), bad_time_count);
    return ok;
}

void uiRunFrameDeliveryTask(const std::atomic<bool>& stopRequested) {
    const int W = 240; 
    const int H = 260; 

    printf("[Controller] UI 帧投递线程启动: 目标尺寸 %dx%d\n", W, H);

    // 局部临时缓冲区，用于从业务层接收数据
    std::vector<uint8_t> temp_buf(W * H * 3);

    while (!stopRequested.load()) {
        // 1. 从业务层获取数据 (存入局部 temp_buf)
        bool ret = business_get_display_frame(temp_buf.data(), W, H);
        
        if (ret) {
            // 2. 将数据推送到 UiManager
            UiManager::getInstance()->updateCameraFrame(temp_buf.data(), temp_buf.size());
        }

        // 简单的帧率控制
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}
