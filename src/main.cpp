/**
 * @file main.cpp
 * @brief 智能考勤系统主程序入口
 * @details 包含系统初始化、单元测试路由以及主循环
 * @version 1.3 (Fix Compilation Error)
 */

#include <iostream>
#include <string>
#include <unistd.h> // for usleep
#include <cstdio>
#include <signal.h>
#include <cstdlib>// 确保包含 system 和 setenv
#include <filesystem>
#include <system_error>

// 1. 引入第三方库头文件
#include "lvgl.h"
#include <opencv2/core.hpp>
#include <sqlite3.h>

// [修复 3] 使用 extern "C" 定义变量，以匹配 ui_app.h 中的声明
// 这样 C 语言 (ui_app.c) 和 C++ (main.cpp) 才能看到同一个变量
extern "C" {
    volatile bool g_program_should_exit = false;
}

// 2. 引入项目模块头文件
#include "ui/ui_app.h"          // UI层
#include "ui/ui_controller.h"
#include "business/face_demo.h" // 业务层
#include "data/db_storage.h"    // 数据层
#include "app/application.h"

namespace {

class UiEmployeeLookupPresenterBinding final {
public:
    explicit UiEmployeeLookupPresenterBinding(
        smart_attendance::ui::EmployeeLookupPresenter& presenter)
        : controller_(*UiController::getInstance()) {
        controller_.configureEmployeeLookupPresenter(&presenter);
    }

    ~UiEmployeeLookupPresenterBinding() {
        controller_.configureEmployeeLookupPresenter(nullptr);
    }

    UiEmployeeLookupPresenterBinding(const UiEmployeeLookupPresenterBinding&) = delete;
    UiEmployeeLookupPresenterBinding& operator=(const UiEmployeeLookupPresenterBinding&) = delete;

private:
    UiController& controller_;
};

std::filesystem::path executableDirectory(const char* executablePath) {
    std::error_code error;
    const auto procExecutable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error && !procExecutable.empty()) {
        return procExecutable.parent_path();
    }

    error.clear();
    const auto absolutePath = std::filesystem::absolute(executablePath, error);
    if (!error) {
        return absolutePath.parent_path();
    }

    return {};
}

const char* initErrorMessage(smart_attendance::app::ApplicationInitError error) {
    using smart_attendance::app::ApplicationInitError;
    switch (error) {
    case ApplicationInitError::InvalidState:
        return "应用生命周期状态无效";
    case ApplicationInitError::InvalidDatabaseLifecycle:
        return "数据库生命周期配置无效";
    case ApplicationInitError::InvalidUiLifecycle:
        return "UI 生命周期配置无效";
    case ApplicationInitError::InvalidApplicationLoop:
        return "应用主循环配置无效";
    case ApplicationInitError::InvalidBusinessLifecycle:
        return "业务生命周期配置无效";
    case ApplicationInitError::RuntimeDirectoryUnavailable:
        return "运行目录无法创建或访问";
    case ApplicationInitError::DatabaseInitializationFailed:
        return "数据库初始化失败";
    case ApplicationInitError::UiInitializationFailed:
        return "UI 初始化失败";
    case ApplicationInitError::None:
        return "无错误";
    }
    return "未知初始化错误";
}

const char* runErrorMessage(smart_attendance::app::ApplicationRunError error) {
    using smart_attendance::app::ApplicationRunError;
    switch (error) {
    case ApplicationRunError::InvalidState:
        return "应用主循环状态无效";
    case ApplicationRunError::LoopFailed:
        return "应用主循环执行失败";
    case ApplicationRunError::None:
        return "无错误";
    }
    return "未知主循环错误";
}

bool exportUserReport(int userId,
                      const std::string& startDate,
                      const std::string& endDate) {
    return UiController::getInstance()->exportUserReport(
        userId, startDate, endDate);
}

bool exportCustomReport(const std::string& startDate,
                        const std::string& endDate) {
    return UiController::getInstance()->exportCustomReport(
        startDate, endDate);
}

bool exportEmployeeSettings() {
    return UiController::getInstance()->exportEmployeeSettings();
}

bool importEmployeeSettings(int& invalidTimeCount) {
    return UiController::getInstance()->importEmployeeSettings(&invalidTimeCount);
}

void initializeUi() {
    std::cout << ">>> 初始化 UI 层..." << std::endl;
    ui_init();
    std::cout << "[OK] UI 层初始化完成" << std::endl;
}

bool shouldStopApplication() {
    return g_program_should_exit;
}

void runUiIteration() {
    uint32_t timeTillNext = lv_timer_handler();
    ui_process_background_results();
    ui_process_system_status();
    if (timeTillNext < 5) {
        timeTillNext = 5;
    }
    if (timeTillNext > 30) {
        timeTillNext = 30;
    }

    usleep(timeTillNext * 1000);
    lv_tick_inc(timeTillNext);
}

} // namespace

// ==========================================
// 测试函数定义区域
// ==========================================


// 捕获 Ctrl+C 信号，防止终端关不掉
void signal_handler(int signum) {
    std::cout << "\n[System] 捕获中断信号 (" << signum << ")，正在退出..." << std::endl;
    g_program_should_exit = true;
}

/**
 * @brief Epic 1 测试: 框架依赖自检
 */
void test_epic1_framework() {
    std::cout << "------------------------------------------" << std::endl;
    std::cout << ">>> [Test] Epic 1: 框架依赖自检" << std::endl;
    std::cout << "[Check] OpenCV Version: " << CV_VERSION << std::endl;
    std::cout << "[Check] SQLite3 Version: " << sqlite3_libversion() << std::endl;
    std::cout << "[Check] LVGL Version: " 
              << lv_version_major() << "." 
              << lv_version_minor() << "." 
              << lv_version_patch() << std::endl;
    std::cout << "[OK] Epic 1 依赖检查完成" << std::endl;
}

// ==========================================
//  强制禁用系统休眠函数
// ==========================================
void disable_system_screensaver() {
    std::cout << ">>> [System] 正在强制禁用屏幕保护和自动休眠..." << std::endl;

    // 1. 设置 SDL 环境变量：明确告诉 SDL 不要调用屏保
    // 0 = Disable screensaver
    setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "0", 1);

    // 2. Linux 控制台命令：禁止黑屏 (Console Blanking)
    // 使用 system() 调用比外部脚本更可靠，因为它随程序启动执行
    int ret = 0;
    
    // 方法 A: setterm (通用)
    // -blank 0: 禁用黑屏
    // -powerdown 0: 禁用电源关闭
    ret = system("setterm -blank 0 -powerdown 0 -powersave off > /dev/null 2>&1");
    
    // 方法 B: 直接向终端发送转义序列 (强制唤醒)
    // \033[9;0] 是 Linux 控制台的“设置休眠时间为0”指令
    ret = system("echo -e '\\033[9;0]' > /dev/tty0 2> /dev/null");
    
    // 方法 C: Framebuffer 直接控制 (如果存在)
    if (access("/sys/class/graphics/fb0/blank", F_OK) == 0) {
        ret = system("echo 0 > /sys/class/graphics/fb0/blank");
    }

    (void)ret; // 忽略返回值警告
}

// ==========================================
// 主程序入口
// ==========================================
int main(int argc, char* argv[]) {
    (void)argc;

    // 注册信号处理 (Ctrl+C)
    signal(SIGINT, signal_handler);

    //  程序启动第一件事：禁用休眠
    disable_system_screensaver();

    std::cout << "==========================================" << std::endl;
    std::cout << "   智能考勤系统 v1.2 - Phase 02" << std::endl;
    std::cout << "==========================================" << std::endl;

    // 1. 基础环境检查
    test_epic1_framework();

    // 2. Application 统一准备运行目录，并依次初始化数据层和 UI 层。
    const auto runtimeDirectory = executableDirectory(argv[0]) / "runtime";
    smart_attendance::app::Application application(
        {data_init, data_close},
        {initializeUi, ui_shutdown},
        {shouldStopApplication, runUiIteration},
        {business_init, business_shutdown},
        exportUserReport,
        exportCustomReport,
        exportEmployeeSettings,
        importEmployeeSettings,
        {uiRunMonitorTask, uiWakeMonitorTask},
        {uiRunFrameDeliveryTask, nullptr},
        {business_run_capture_task, business_wake_capture_task},
        {business_run_database_writer_task,
         business_wake_database_writer_task},
        runtimeDirectory);
    business_configure_punch_service(application.punchService());
    ui_configure_background_jobs(application.uiBackgroundJobs());
    ui_configure_system_status(application.uiSystemStatus());

    std::cout << ">>> 初始化数据层..." << std::endl;
    const auto initResult = application.initialize();
    if (initResult != smart_attendance::app::ApplicationInitError::None) {
        std::cerr << "[Fatal] " << initErrorMessage(initResult) << "，程序退出。" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[OK] 运行时文件目录: "
              << application.runtimeDirectory().string() << std::endl;
    UiEmployeeLookupPresenterBinding employeeLookupPresenterBinding(
        application.employeeLookupPresenter());

    // 3. 后初始化业务层 (再启动线程，确保发出的第一个请求都有消费者)
    std::cout << ">>> 初始化业务层..." << std::endl;
    if (!application.markRunning()) {
        std::cerr << "[Fatal] 业务层初始化失败或应用生命周期状态异常。" << std::endl;
        application.requestStop();
        application.stop();
        return EXIT_FAILURE;
    }

    // 4. 进入主循环
    std::cout << ">>> 系统主循环启动" << std::endl;
    const auto runResult = application.run();
    if (runResult == smart_attendance::app::ApplicationRunError::None) {
        std::cout << ">>> 系统安全退出 (Main Loop Ended)" << std::endl;
    } else {
        std::cerr << "[Error] " << runErrorMessage(runResult) << "。" << std::endl;
    }

    const bool stopRequested = application.requestStop();
    const bool stopped = stopRequested && application.stop();
    if (!stopped) {
        std::cerr << "[Error] 后台任务或数据库关闭失败。" << std::endl;
        return EXIT_FAILURE;
    }
    return runResult == smart_attendance::app::ApplicationRunError::None
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
