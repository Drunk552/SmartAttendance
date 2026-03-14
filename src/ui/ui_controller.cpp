/**
 * @file ui_controller.cpp
 * @brief UI 控制器实现文件 - 提供 UI 层与业务/数据层的接口封装
 * @details 该类封装了 UI 层所需的各种业务逻辑调用，简化 UI 代码复杂度。
 *          通过单例模式提供全局访问点。
 */
#include "ui_controller.h"
// 引入原来 ui_app.cpp 依赖的底层头文件
#include "../data/db_storage.h"
#include "../business/face_demo.h"
#include "../business/report_generator.h"
#include "../business/attendance_rule.h"
#include "../business/event_bus.h"
#include "managers/ui_manager.h"
#include <sys/statvfs.h>
#include <algorithm>
#include <set>
#include <cstdio>
#include <filesystem>
#include <thread> // for sleep if needed
#include <cstring> // memcpy
#include <sstream>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;// C++17 引入的文件系统库

static UiController* s_instance = nullptr;

UiController* UiController::getInstance() {
    if (!s_instance) {
        s_instance = new UiController();
    }
    return s_instance;
}

// 移入原 check_disk_low 逻辑
bool UiController::isDiskFull() {
    struct statvfs stat;
    if (statvfs(".", &stat) != 0) return false;
    unsigned long long free_bytes = stat.f_bavail * stat.f_frsize;
    unsigned long long free_mb = free_bytes / (1024 * 1024);
    return (free_mb < 100);
}

// 移入原 get_current_time_str 逻辑
std::string UiController::getCurrentTimeStr() {
    std::time_t rawtime = std::time(nullptr);
    std::tm *timeinfo = std::localtime(&rawtime);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M", timeinfo);
    return std::string(buf);
}

// 获取当前星期几字符串实现
std::string UiController::getCurrentWeekdayStr() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[16];
    localtime_r(&now, &tstruct);
    
    // %a 表示星期的缩写 (如: Mon, Tue, Wed...)
    // 如果你想要全称用 %A (如: Monday)，但考虑到顶部空间有限(60px)，建议用缩写
    strftime(buf, sizeof(buf), "%a", &tstruct);
    
    return std::string(buf);
}

// 移入原 get_next_available_id 逻辑
int UiController::generateNextUserId() {
    std::vector<UserData> users = db_get_all_users();
    
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
    return db_get_departments();
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

bool UiController::registerNewUser(const std::string& name, int deptId) {
    // 调用业务层接口
    return business_register_user(name.c_str(), deptId);
}

int UiController::getUserRoleById(int userId) {
    // 1. Controller 层调用数据层，拿到一个 optional "盒子"
    auto user_opt = db_get_user_info(userId);
    
    // 2. 检查盒子是否为空（找不到该用户）
    if (!user_opt.has_value()) {
        return -1; // 如果为空，返回 -1
    }
    
    // 3. 拆盒取数据并返回权限值
    return user_opt.value().role;
}

// 验证用户密码是否正确（哈希验证）
bool UiController::verifyUserPassword(int userId, const std::string& inputPassword) {
    // 1. 获取用户信息
    UserData user = getUserInfo(userId);
    
    // 2. 如果用户不存在或没设密码，直接返回 false
    if (user.password.empty()) {
        return false; 
    }

    // 3. 调用数据层暴露的哈希函数，将输入的明文转为哈希值
    std::string hashed_input = db_hash_password(inputPassword);

    // 4. 比对结果
    return (user.password == hashed_input);
}

std::vector<UserData> UiController::getAllUsers() {
    return db_get_all_users();
}

int UiController::getUserCount() {
    return business_get_user_count();
}

bool UiController::getUserAt(int index, int* id, char* name_buf, int buf_len) {
    return business_get_user_at(index, id, name_buf, buf_len);
}

UserData UiController::getUserInfo(int uid) {
    auto user_opt = db_get_user_info(uid);
    
    // 如果查到了，拆盒把真实数据返回给 UI 层
    if (user_opt.has_value()) {
        return user_opt.value();
    } 
    
    // 如果没查到（空盒子），为了兼容原有 UI 逻辑，构造一个 id=0 的空对象
    UserData empty_user;
    empty_user.id = 0;
    //设一个默认名字防止界面显示乱码
    empty_user.name = "Unknown"; 
    
    return empty_user;
}

// 检查用户是否存在 (用于 UI 导出报表前的同步校验)
bool UiController::checkUserExists(int user_id) {
    // 1. 获取 optional "盒子"
    auto user_opt = db_get_user_info(user_id);
    
    // 2. 直接判断盒子是否有值即可，有值代表存在，没值代表不存在
    return user_opt.has_value();
}

std::vector<AttendanceRecord> UiController::getRecords(int userId, time_t start, time_t end) {
    // 可以在这里对数据进行过滤，UI层拿到的就是处理好的
    auto all_records = db_get_records(start, end);
    if (userId < 0) return all_records; // 约定 < 0 返回所有

    std::vector<AttendanceRecord> filtered;
    for (const auto& rec : all_records) {
        if (rec.user_id == userId) filtered.push_back(rec);
    }
    return filtered;
}

bool UiController::exportReportToUsb() {
    // 将原 ui_download_report_handler 中的逻辑移到这里
    // 包括创建目录、计算日期、调用 Generator
    // 这里只保留纯业务，不包含 lv_msgbox 等 UI 弹窗代码
    // 可以返回一个状态码或 bool 给 UI 层去决定弹窗内容
    try {
        std::filesystem::create_directories("output/usb_sim");
    } catch (...) { return false; }

    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    char start_date[16], end_date[16];
    std::strftime(start_date, sizeof(start_date), "%Y-%m-01", now);
    std::strftime(end_date, sizeof(end_date), "%Y-%m-%d", now);

    ReportGenerator generator;
    return generator.exportAllAttendanceReport(start_date, end_date, "output/usb_sim/attendance_report.xlsx");
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
    // 1. 先获取当前用户的 optional "盒子"
    auto user_opt = db_get_user_info(userId);
    
    // 2. 检查用户是否存在 (盒子为空直接返回失败)
    if (!user_opt.has_value()) return false; 

    // 3. 拆盒取出真实数据
    UserData user = user_opt.value();

    // 4. 调用底层更新接口：保留原有的 dept_id, role, card_id 不变，只修改 name
    return db_update_user_basic(userId, newName, user.dept_id, user.role, user.card_id);
}

//更新用户部门信息
bool UiController::updateUserDept(int userId, int newDeptId) {
    // 1. 先获取该用户当前所有的基本信息
    UserData currentUser = getUserInfo(userId);
    
    // 2. 调用 db_update_user_basic，只把 dept_id 替换成新的，其他用旧的
    return db_update_user_basic(userId, currentUser.name, newDeptId, currentUser.role, currentUser.card_id);
}

//更新用户人脸
bool UiController::updateUserFace(int userId) {
    // 直接调用业务层刚刚封装好的接口！
    // 业务层会自动抓取当前画面、更新数据库，并刷新识别模型。
    return business_update_user_face(userId);
}

// 更新用户密码实现
bool UiController::updateUserPassword(int userId, const std::string& newPassword) {
    // 底层有单独修改密码的接口，直接调用即可
    return db_update_user_password(userId, newPassword);
}

// 更新用户权限实现
bool UiController::updateUserRole(int userId, int newRole) {
    // 1. 获取当前用户的 optional "盒子"
    auto user_opt = db_get_user_info(userId);
    
    // 2. 检查用户是否存在
    if (!user_opt.has_value()) return false;

    // 3. 拆盒取出真实数据
    UserData user = user_opt.value();

    // 4. 调用底层更新接口：保留原有的 name, dept_id, card_id 不变，只修改 role
    return db_update_user_basic(userId, user.name, user.dept_id, newRole, user.card_id);
}

// 删除用户实现
bool UiController::deleteUser(int userId) {
    // 调用底层数据库接口
    // 注意：db_delete_user 会级联删除该用户的考勤记录和图片，非常干净
    return db_delete_user(userId);
}

// 导出自定义全体员工报表实现
bool UiController::exportCustomReport(const std::string& start, const std::string& end) {
    std::string dir = "output/usb_sim";
    
    // 强制创建目录，如果目录不存在
    if (!fs::exists(dir)) {
        fs::create_directories(dir); 
    }

    std::string path = dir + "/Attendance_Report_All_" + start + "_to_" + end + ".xlsx";
    ReportGenerator report_gen; 
    return report_gen.exportAllAttendanceReport(start, end, path);
}

// 导出个人报表实现
bool UiController::exportUserReport(int user_id, const std::string& start, const std::string& end) {
    std::string dir = "output/usb_sim";

    // 强制创建目录，如果目录不存在
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }

    std::string path = dir + "/User_" + std::to_string(user_id) + "_Report.xlsx";
    ReportGenerator report_gen;
    return report_gen.exportIndividualAttendanceReport(user_id, start, end, path);
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
    // 直接调用数据层接口并返回
    return db_get_system_stats();
}

void UiController::clearAllRecords() {
    db_clear_attendance();
}

// 恢复出厂设置实现
void UiController::factoryReset() {
    // 调用底层数据层的重置接口
    db_factory_reset();
}

// 清除所有员工实现 (防止下一个报错是它)
void UiController::clearAllEmployees() {
    db_clear_users();
}

// 清除所有数据实现
void UiController::clearAllData() {
    // 假设底层有这个函数，或者手动调用清除员工+清除记录
    db_clear_users();
    db_clear_attendance();
    // 可能还需要删除特征文件等，视具体业务而定
}

// 实现导出员工设置表功能Set the table
bool UiController::exportEmployeeSettings() {
    std::string dir = "output/usb_settings";

    // 强制创建目录，如果目录不存在
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }

    ReportGenerator generator;
    
    std::string export_path = dir + "/员工设置表.xlsx"; 
    
    return generator.exportSettingsReport(export_path);
}

// 后台服务与事件总线
void UiController::startBackgroundServices() {
    if (m_running) return;
    m_running = true;

    // 1. 启动系统监控线程
    m_monitor_thread = std::thread(&UiController::monitorThreadFunc, this);
    m_monitor_thread.detach();

    // 2. 启动摄像头采集线程
    m_capture_thread = std::thread(&UiController::captureThreadFunc, this);
    m_capture_thread.detach();
}

// 监控线程实现
void UiController::monitorThreadFunc() {
    while (m_running) {
        // A. 时间事件 (每秒)
        std::string timeStr = getCurrentTimeStr();
        EventBus::getInstance().publish(EventType::TIME_UPDATE, &timeStr);

        // B. 磁盘监控 (每 5 秒)
        static int disk_check_counter = 0;
        if (++disk_check_counter >= 5) {
            disk_check_counter = 0;
            if (isDiskFull()) EventBus::getInstance().publish(EventType::DISK_FULL);
            else EventBus::getInstance().publish(EventType::DISK_NORMAL);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// ============================================================
// 上传员工设置表实现
// 流程：解压 output/usb_settings/员工设置表.xlsx
//       → 读取 sharedStrings.xml 建立字符串索引
//       → 读取 sheet1.xml 第6行起解析员工数据(工号/姓名/部门/权限)
//       → 调用 db_batch_add_users 写入数据库
// ============================================================
bool UiController::importEmployeeSettings(int* invalid_time_count) {
    const std::string xlsx_path = "output/usb_settings/员工设置表.xlsx";
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

    // 逐行解析：匹配 <row r="N" ...>...</row>（N >= 6 为数据行）
    std::regex row_re("<row r=\"(\\d+)\"[^>]*>([\\s\\S]*?)</row>");
    // 匹配行内每个单元格：<c r="A6" t="s"><v>3</v></c>（t 可能不存在）
    std::regex cell_re("<c r=\"([A-Z]+)(\\d+)\"(?:[^>]* t=\"([^\"]*)\")?[^>]*>(?:<v>([^<]*)</v>)?");

    auto row_begin = std::sregex_iterator(sheet_xml.begin(), sheet_xml.end(), row_re);
    auto row_end   = std::sregex_iterator();

    for (auto row_it = row_begin; row_it != row_end; ++row_it) {
        int row_num = std::stoi((*row_it)[1].str());
        if (row_num < 6) continue; // 前5行是表头，跳过

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

        // A(1)=工号, B(2)=姓名, C(3)=部门ID, D(4)=权限
        // 工号和姓名是必填字段，缺少则跳过
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

        import_users.push_back(u);
        printf("[Import] 读取员工: id=%d name=%s dept=%d role=%d\n",
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
            int mins = AttendanceRule::timeStringToMinutes(raw);
            if (mins < 0) {
                // 非法格式：记录日志并计数，返回空串（写DB时跳过）
                printf("[Import] 时间格式非法，已跳过: \"%s\"\n", raw.c_str());
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
            db_update_shift(shift_id, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end, 0);
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
    bool ok = db_batch_add_users(import_users);
    printf("[Import] 导入结果: %s,共 %zu 名员工，时间格式异常字段: %d\n",
           ok ? "成功" : "失败", import_users.size(), bad_time_count);
    return ok;
}

// 摄像头采集线程
void UiController::captureThreadFunc() {
    const int W = 240; 
    const int H = 260; 

    printf("[Controller] 采集线程启动: 目标尺寸 %dx%d\n", W, H);

    // 局部临时缓冲区，用于从业务层接收数据
    std::vector<uint8_t> temp_buf(W * H * 3);

    while (m_running) {
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