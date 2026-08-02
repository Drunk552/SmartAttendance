/**
 * @file application.h
 * @brief 声明整机应用的最小生命周期和数据库资源所有权。
 */

#ifndef SMART_ATTENDANCE_APP_APPLICATION_H
#define SMART_ATTENDANCE_APP_APPLICATION_H

#include <filesystem>

namespace smart_attendance::app {

enum class ApplicationState {
    Created,
    Initialized,
    Running,
    StopRequested,
    Stopped
};

enum class ApplicationInitError {
    None,
    InvalidState,
    InvalidDatabaseLifecycle,
    RuntimeDirectoryUnavailable,
    DatabaseInitializationFailed
};

/**
 * @brief 旧数据层接入 Application 的过渡适配接口。
 *
 * 两个回调均由 Application 调用；调用方必须保证回调在 Application 生命周期内有效。
 */
struct DatabaseLifecycle {
    bool (*initialize)();
    void (*close)();
};

class Application final {
public:
    /**
     * @brief 创建应用组合根，但不立即访问文件系统或数据库。
     * @param databaseLifecycle 数据库初始化和关闭回调，Application 不拥有回调目标。
     * @param runtimeDirectory 运行时数据目录；初始化成功后成为进程工作目录。
     */
    Application(DatabaseLifecycle databaseLifecycle,
                std::filesystem::path runtimeDirectory);
    ~Application() noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /**
     * @brief 创建运行目录并初始化唯一的数据库连接。
     * @return 成功返回 None；失败返回具体原因且状态保持 Created。
     *
     * @note 该函数修改进程工作目录，因此必须在 UI、业务线程和文件访问启动前调用。
     */
    ApplicationInitError initialize() noexcept;

    /** @brief 将应用从 Initialized 转换为 Running。 */
    bool markRunning() noexcept;

    /**
     * @brief 请求停止应用。
     * @note 当前阶段只记录停止请求，后台线程仍由现有 business_quit() 负责回收。
     */
    bool requestStop() noexcept;

    /**
     * @brief 在后台线程停止后关闭数据库并进入 Stopped 状态。
     * @return 关闭成功返回 true；状态不合法或关闭回调异常时返回 false。
     */
    bool stop() noexcept;

    ApplicationState state() const noexcept;
    const std::filesystem::path& runtimeDirectory() const noexcept;

private:
    void closeDatabaseNoexcept() noexcept;

    DatabaseLifecycle databaseLifecycle_;
    std::filesystem::path runtimeDirectory_;
    ApplicationState state_{ApplicationState::Created};
    bool databaseInitialized_{false};
};

} // namespace smart_attendance::app

#endif // SMART_ATTENDANCE_APP_APPLICATION_H
