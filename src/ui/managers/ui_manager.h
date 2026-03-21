#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include <array>
#include <vector>
#include <atomic>
#include <functional>
#include <mutex>
#include "../common/ui_style.h"

// 定义屏幕类型枚举，用于管理
enum class ScreenType {
    MAIN,//home屏幕
    MENU,//主菜单
    USER_MGMT,//员工管理主菜单
    USER_LIST,//员工列表子屏幕
    REGISTER,// 注册流程屏幕
    REGISTER_CAMERA,// 注册流程拍照屏幕
    RECORD_QUERY,// 记录查询菜单
    SYS_SETTINGS,// 系统设置
    SYS_BASIC,//系统基础设置
    SYS_BASIC_SETTIME,//系统基础-时间设置
    SYS_BASIC_DATE,//系统基础-日期
    SYS_BASIC_SETDATE,//系统基础-日期设置
    SYS_BASIC_DATE_FORMAT,//系统基础-日期格式
    SYS_BASIC_VOLUME,//系统基础-音量设置
    SYS_BASIC_LANGUAGE,//系统基础-语言设置
    SYS_BASIC_SCREENSAFE,//系统基础-屏保时间设置
    SYS_BASIC_MACHINE_ID,//系统基础-设备号设置
    SYS_BASIC_RETURN_TIME,//系统基础-返回主界面时间设置
    SYS_BASIC_ADMIN_COUNT,//系统基础-管理员总数
    SYS_BASIC_WARN_COUNT,//系统基础-警告数
    SYS_ADVANCED,// 系统高级设置
    SYS_ADV_CLEAR_RECORDS,    // 清空记录界面
    SYS_ADV_CLEAR_EMPLOYEES,  // 清空员工界面
    SYS_ADV_CLEAR_ALL_DATA,   // 清空所有数据界面
    SYS_ADV_FACTORY_RESET,    // 恢复出厂设置界面
    SYS_ADV_UPGRADE,           // 系统升级界面
    SYS_PARAM,//系统参数设置
    SYS_SELFCHECK,//系统自检功能设置
    SYS_INFO,// 系统信息
    STORAGE_INFO,// 存储信息
    FACILITY_INFO,//设备信息
    ATT_STATS,// 考勤统计
    ALL_ATT_STATS,//全员考勤报表下载
    PERSONAGE_ATT_STATS,//个人考勤报表下载
    ATT_DESIGN,// 考勤设计
    DEPT_SETTING, //部门设置子界面
    SHIFT_SETTING,//班次设置子界面
    SHIFT_INFO,//班次详细信息界面
    RULE_SETTING,//考勤规则设置
    SCHEDULE_SETTING,//排班设置
    USER_INFO, // 员工详情
    PWD_CHANGE,// 密码修改
    ROLE_AUTH,// 权限变更
    DELETE_USER,//删除用户
    SYS_SELF_CHECK, // 自检功能屏幕
    USER_EDIT_NAME,//修改姓名
    USER_EDIT_DEPT,//修改部门
    USER_EDIT_PASSWORD,//修改密码
    USER_REGISTER_PASSWORD,//注册密码
    JOB_QUERY,//工号查询
    BROWSE_JOB_QUERY,//浏览工号查询
    BROWSE_JOB_QUERY_RESULT,//浏览工号查询结果
    TIME_QUERY,//时间查询界面
    DOWNLOAD_JOB_QUERY,//下载工号查询界面
    BROWSE_TIME_QUERY_RESULT,//浏览时间查询结果
    DOWNLOAD_TIME_QUERY,//下载时间查询界面
    BELL_SETTING,           // 定时响铃子界面
    COMPANY_SETTING,        // 公司设置子界面
    DEPT_DETAIL,            // 部门详情子界面
    RECORD_RESULT,          // 记录查询结果
    // ... 其他屏幕类型
    // 注意：每添加一个新屏幕类型，都要在 UiManager::registerScreen 中添加对应的管理逻辑
    // ... 其他屏幕
    UNKNOWN
};

class UiManager {
public:
    static UiManager* getInstance();

    // 初始化
    void init();

    // --- 输入设备管理 ---
    lv_group_t* getKeypadGroup() { return g_keypad_group; }
    void resetKeypadGroup(); // 清空并重置组
    void addObjToGroup(lv_obj_t* obj);

    // --- 摄像头缓冲区管理 (线程安全共享) ---
    // 获取显示缓冲区指针 (供 lv_image_dsc_t 使用)
    uint8_t* getCameraDisplayBuffer() { return cam_buf_display.data(); }
    size_t getCameraDisplayBufferSize() { return cam_buf_display.size(); }
    
    // 标记有一帧待更新 (原子操作)
    bool trySetFramePending() {
        bool expected = false;
        return s_ui_frame_pending.compare_exchange_strong(expected, true);
    }
    
    /**
     * @brief 更新摄像头帧数据到显示缓冲区
     * @param data 指向新帧数据的指针
     * @param size 数据大小（字节）
     */
    void updateCameraFrame(const uint8_t* data, size_t size);

    // 清除帧待更新标记
    void clearFramePending() { s_ui_frame_pending.store(false); }

    // --- 屏幕管理与内存清理 (核心) ---
    
    /**
     * @brief 注册一个屏幕对象到管理器
     * 在 create_xxx_screen 时调用此函数，让 Manager 知道该屏幕的存在
     */
    void registerScreen(ScreenType type, lv_obj_t** screen_ptr_ref);

    /**
     * @brief 启动异步销毁任务 (防崩溃核心)
     * 销毁除 keep_scr 以外的所有屏幕
     */
    void destroyAllScreensExcept(lv_obj_t* keep_scr);

    /**
     * @brief 清理特定屏幕的资源 (内部调用)
     */
    void freeScreenResources(lv_obj_t** screen_ptr);

private:
    UiManager();
    ~UiManager() = default;

    // 禁用拷贝
    UiManager(const UiManager&) = delete;
    UiManager& operator=(const UiManager&) = delete;

    // 全局输入组
    lv_group_t* g_keypad_group = nullptr;

    // 摄像头显示缓冲区
    std::array<uint8_t, CAM_W * CAM_H * 3> cam_buf_display;
    
    // 帧同步原子标志
    std::atomic<bool> s_ui_frame_pending{false};

    // 互斥锁，保护画面数据
    std::mutex m_frame_mutex;

    // 屏幕管理列表
    // 存储指向 "全局屏幕指针变量" 的指针
    // 例如: &screen_main, &screen_menu
    struct ManagedScreen {
        ScreenType type;
        lv_obj_t** ptr_ref; // 指向 static lv_obj_t* screen_xxx 的指针
    };
    std::vector<ManagedScreen> managed_screens;

    // 定时器回调
    static void async_screen_cleanup_cb(lv_timer_t* t);
};

#endif // UI_MANAGER_H