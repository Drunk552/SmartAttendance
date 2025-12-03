#include <iostream>
#include <unistd.h> // for usleep
#include <vector>

// 1. 引入第三方库头文件 (验证头文件路径配置是否正确)
#include "lvgl.h"
#include <opencv2/core.hpp>
#include <sqlite3.h>

// 2. 引入我们自己的模块头文件 (验证内部模块链接是否正确)
#include "ui/ui_app.h"          // 对应 src/ui/ui_app.c
#include "business/face_demo.h" // 对应 src/business/face_demo.cpp
#include "data/db_storage.h"  // <--- [12.2日新增] 引入数据层头文件

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "   智能考勤系统 - 框架启动 & 依赖自检" << std::endl;
    std::cout << "==========================================" << std::endl;

    // ---------------------------------------------------------
    // 第一步：验证第三方依赖库 (验收标准：依赖已正确配置并链接)
    // ---------------------------------------------------------
    
    // 1. 验证 OpenCV
    std::cout << "[Check 1] OpenCV Version: " << CV_VERSION << std::endl;
    
    // 2. 验证 SQLite3
    std::cout << "[Check 2] SQLite3 Version: " << sqlite3_libversion() << std::endl;

    // 3. 验证 LVGL (通过调用 lv_init)
    // 注意：UI初始化通常已经包含了 lv_init，但这里打印版本确认一下
    std::cout << "[Check 3] LVGL Version: " 
              << lv_version_major() << "." 
              << lv_version_minor() << "." 
              << lv_version_patch() << std::endl;

    std::cout << "------------------------------------------" << std::endl;

    //[12.2新增]Epic 2 数据层独立测试（验证标准：能通过独立测试）
    //----------------------------------------------------------
    std::cout<< ">>>[Test] 初始化数据库..." << std::endl;
    //1.测试初始化接口
    if(data_init()){
        std::cout << "[OK] 数据库连接成功" << std::endl;

        //2.创建一个测试图像（纯黑色 100x100）
        cv::Mat test_img = cv::Mat::zeros(100,100,CV_8UC3);

        std::cout << ">>>[Test] 尝试保存测试图像..." << std::endl;
        //3.测试保存接口
        if(data_saveImage(test_img)){
            std::cout << "[OK] data_saveImage 测试通过！" << std::endl;
        } else {
            std::cerr << "[Failed] data_saveImage 测试失败！" << std::endl;
        }
    } else {
        std::cerr << "[Failed] 数据库初始化失败！" << std::endl;
    }
    std::cout << "------------------------------------------" << std::endl;

    // ---------------------------------------------------------
    // 第二步：初始化各层模块 (验证三层架构集成)
    // ---------------------------------------------------------

    // A. 初始化 UI 层
    std::cout << ">>> 正在初始化 UI 层..." << std::endl;
    ui_init(); 
    std::cout << "[OK] UI 层初始化完成" << std::endl;

    // B. 初始化 业务层
    std::cout << ">>> 正在初始化 业务层..." << std::endl;
    if (!business_init()) {
        std::cerr << "[Warning] 业务层初始化遇到问题 (如摄像头未连接)，但在模拟环境中继续运行..." << std::endl;
        // 在正式产品中可能需要 return -1; 但开发阶段允许继续跑 UI
    } else {
        std::cout << "[OK] 业务层初始化完成" << std::endl;
    }

    // ---------------------------------------------------------
    // 第三步：进入主循环 (The Super Loop)
    // ---------------------------------------------------------
    std::cout << ">>> 系统主循环启动" << std::endl;
    
    while (1) {
        // 1. 驱动 LVGL (UI刷新、动画、输入响应)
        // lv_timer_handler 返回距离下一次任务还需要多少毫秒
        uint32_t time_till_next = lv_timer_handler();

        // 2. 驱动 业务逻辑 (人脸检测、识别)
        business_run_once();

        // 3. 线程休眠
        // 为了平衡 CPU 占用和 UI 流畅度
        // 如果 LVGL 没什么事做，我们可以多睡会儿，但不能超过 time_till_next
        // 同时为了保证摄像头帧率，休眠时间不宜过长 (例如 5ms ~ 10ms)
        if (time_till_next > 5) time_till_next = 5;
        usleep(time_till_next * 1000); 
    }

    // 程序退出前清理 (虽然 while(1) 永远不会到这，但这是好习惯)
    data_close();
    
    return 0;
}