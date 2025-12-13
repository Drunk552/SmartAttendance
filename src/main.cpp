#include <iostream>
#include <fstream>      // 添加这行，用于文件流操作
#include <string>       // 添加这行，用于字符串操作
#include <unistd.h> // for usleep
#include <vector>
#include <cstdio>       // 添加这行，用于C风格文件操作

// 1. 引入第三方库头文件 (验证头文件路径配置是否正确)
#include "lvgl.h"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp> 
#include <opencv2/highgui.hpp>   
#include <sqlite3.h>

// 2. 引入我们自己的模块头文件 (验证内部模块链接是否正确)
#include "ui/ui_app.h"          // 对应 src/ui/ui_app.c
#include "business/face_demo.h" // 对应 src/business/face_demo.cpp
#include "data/db_storage.h"  // 引入数据层头文件

// ==========================================
// 测试函数定义区域
// ==========================================

/**
 * @brief Epic 1 测试: 框架依赖自检
 * 验证 OpenCV, SQLite3, LVGL 是否正确链接
 */
void test_epic1_framework() {
    std::cout << "------------------------------------------" << std::endl;
    std::cout << ">>> [Test] Epic 1: 框架依赖自检" << std::endl;
    
    // 验证 OpenCV
    std::cout << "[Check] OpenCV Version: " << CV_VERSION << std::endl;
    
    // 验证 SQLite3
    std::cout << "[Check] SQLite3 Version: " << sqlite3_libversion() << std::endl;

    // 验证 LVGL
    std::cout << "[Check] LVGL Version: " 
              << lv_version_major() << "." 
              << lv_version_minor() << "." 
              << lv_version_patch() << std::endl;
    std::cout << "[OK] Epic 1 依赖检查完成" << std::endl;
}

/**
 * @brief Epic 2 测试: 数据层独立测试 (混合存储适配版)
 * 验证数据库初始化、用户注册(BLOB)和考勤记录(Path)功能
 */
void test_epic2_data_layer() {
    std::cout << "------------------------------------------" << std::endl;
    std::cout << ">>> [Test] Epic 2: 数据层独立测试" << std::endl;

    std::cout << ">>> 初始化数据库..." << std::endl;
    
    if (data_init()) {
        std::cout << "[OK] 数据库连接成功" << std::endl;

        // 创建一个测试图像 (纯黑色 100x100)
        // 注意：LBPH通常需要灰度图，我们这里直接创建单通道灰度图
        cv::Mat test_img = cv::Mat::zeros(100, 100, CV_8UC1);
        
        // -------------------------------------------------
        // 测试 1: 用户注册 (存入 users 表，BLOB)
        // -------------------------------------------------
        std::cout << ">>> [1] 尝试注册测试用户 (BLOB)..." << std::endl;
        int new_uid = data_registerUser("Test_User_Epic2", test_img);
        
        if (new_uid != -1) {
            std::cout << "[OK] data_registerUser 测试通过!新用户ID: " << new_uid << std::endl;

            // -------------------------------------------------
            // 测试 2: 保存考勤 (存入 attendance 表，Path)
            // -------------------------------------------------
            std::cout << ">>> [2] 尝试保存考勤记录 (Path)..." << std::endl;
            // 使用刚才注册成功的 new_uid 来记录考勤
            if (data_saveAttendance(new_uid, test_img)) {
                std::cout << "[OK] data_saveAttendance 测试通过！" << std::endl;
            } else {
                std::cerr << "[Failed] data_saveAttendance 测试失败！" << std::endl;
            }

        } else {
            std::cerr << "[Failed] data_registerUser 测试失败！" << std::endl;
        }

    } else {
        std::cerr << "[Failed] 数据库初始化失败！" << std::endl;
    }
}

/**
 * @brief 测试business_processAndSaveImage接口
 * @param imagePath 测试图像路径
 * @return 测试是否通过
 */
bool test_epic3_business_processAndSaveImage(const std::string& imagePath) {
    std::cout << "=== 开始测试business_processAndSaveImage接口 ===" << std::endl;
        
    cv::Mat testImage = cv::imread(imagePath);
    if (testImage.empty()) {
        std::cerr << "测试失败：无法加载测试图像 " << imagePath << std::endl;
        return false;
    }//读取本地图片

    std::cout << "测试图像加载成功，尺寸: " << testImage.cols << "x" << testImage.rows << std::endl;
    
    std::cout << "正在调用business_processAndSaveImage接口..." << std::endl;
    bool result = business_processAndSaveImage(testImage);//调用业务层接口
    
    if (result) {
        std::cout << " 业务层接口测试通过" << std::endl;
    } 
    else {
        std::cerr << "业务层接口测试失败" << std::endl;
    }// 验证结果
    
    std::cout << "=== 测试完成 ===" << std::endl;
    return result;
}

/**
 * @brief 测试convertToGrayscale函数
 * @param imagePath 测试图像路径
 * @return 测试是否通过
 */
bool test_epic3_convertToGrayscale(const std::string& imagePath) {
    std::cout << "=== 开始测试convertToGrayscale函数 ===" << std::endl;
    
    cv::Mat testImage = cv::imread(imagePath);
    if (testImage.empty()) {
        std::cerr << "测试失败：无法加载测试图像" << std::endl;
        return false;
    }

    cv::Mat grayImage = convertToGrayscale(testImage);
    if (grayImage.empty()) {
        std::cerr << "BGR转灰度测试失败" << std::endl;
        return false;
    } // 测试BGR图像转换

    std::cout << "BGR转灰度测试成功,灰度图尺寸: " << grayImage.cols << "x" << grayImage.rows << "，通道数: " << grayImage.channels() << std::endl;

    cv::Mat grayCopy = convertToGrayscale(grayImage);
    if (grayCopy.empty()) {
        std::cerr << "灰度图转灰度测试失败" << std::endl;
        return false;
    }// 测试灰度图像转换

    std::cout << "灰度图转灰度测试成功" << std::endl;
  
    std::cout << "=== 测试完成 ===" << std::endl;
    return true;
}

/**
 * @brief Epic 5 集成测试：验证完整流程
 * 验证从UI捕获到数据库保存的完整流程
 */
void test_epic5_integration() {
    std::cout << "------------------------------------------" << std::endl;
    std::cout << ">>> [Test] Epic 5: 系统集成测试" << std::endl;
    
    // 验证1：检查数据库文件是否存在
    std::ifstream db_file("attendance.db");
    if (db_file.good()) {
        std::cout << "[OK] 数据库文件 attendance.db 存在" << std::endl;
    } else {
        std::cerr << "[WARN] 数据库文件 attendance.db 不存在" << std::endl;
    }
    
    // 验证2：检查表结构
    std::cout << ">>> 集成测试提示：" << std::endl;
    std::cout << "1. 请确保摄像头已连接" << std::endl;
    std::cout << "2. 运行程序后,点击UI中的'拍照/Capture'按钮" << std::endl;
    std::cout << "3. 观察控制台输出是否包含'图像已保存,ID:'" << std::endl;
    std::cout << "4. 使用以下命令检查数据库：" << std::endl;
    std::cout << "   sqlite3 attendance.db \"SELECT * FROM processed_images;\"" << std::endl;
}

// ==========================================
// 主程序入口
// ==========================================
int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "   智能考勤系统 - 启动" << std::endl;
    std::cout << "==========================================" << std::endl;

    // ---------------------------------------------------------
    // 第一步：执行单元测试 (Epic 1 & 2)
    // ---------------------------------------------------------
    test_epic1_framework();
    test_epic2_data_layer();

    std::cout << "------------------------------------------" << std::endl;
    std::cout << ">>> 测试阶段结束，进入系统初始化..." << std::endl;

    // ---------------------------------------------------------
    // 第二步：按正确顺序初始化各层
    // ---------------------------------------------------------  
    
    // ---------------------------------------------------------
    // 必须先初始化业务层，才能进行 Epic 3 的业务测试
    // ---------------------------------------------------------
    std::cout << ">>> 正在初始化 业务层..." << std::endl;
    if (!business_init()) {
        std::cerr << "[Error] 业务层初始化失败，程序退出。" << std::endl;
        data_close(); // 清理资源
        return -1; 
    }
    std::cout << "[OK] 业务层初始化完成" << std::endl;
    
    std::cout << ">>> 执行 Epic 3 业务功能测试..." << std::endl;
    std::string testImagePath = "./test_images/test1.jpg";//测试图像路径

    bool testResult1 = test_epic3_business_processAndSaveImage(testImagePath);//测试business_processAndSaveImage接口
    bool testResult2 = test_epic3_convertToGrayscale(testImagePath);//测试convertToGrayscale函数

    if(testResult1 && testResult2) {
     std::cout << "[All Tests Passed] 业务层功能测试全部通过" << std::endl;//所有测试通过
    }

    else {
        std::cerr << "[Some Tests Failed] 业务层功能测试存在失败项" << std::endl;//部分测试未通过  
        if (! testResult1)std::cerr << " - business_processAndSaveImage测试未通过" << std::endl;//测试business_processAndSaveImage接口
        if (! testResult2)std::cerr << " - convertToGrayscale测试未通过" << std::endl;//测试convertToGrayscale函数
}

    // ---------------------------------------------------------
    // 第三步：初始化UI并进入主循环
    // ---------------------------------------------------------

    // 初始化 UI 层
    std::cout << ">>> 正在初始化 UI 层..." << std::endl;
    ui_init(); 
    std::cout << "[OK] UI 层初始化完成" << std::endl;

    // ---------------------------------------------------------
    // 执行测试5
    // ---------------------------------------------------------
    test_epic5_integration();
    
    // ---------------------------------------------------------
    // 第五步：进入主循环 (The Super Loop)
    // ---------------------------------------------------------
    std::cout << ">>> 系统主循环启动" << std::endl;
    std::cout << ">>> 操作提示: 在UI窗口中点击'拍照/Capture'按钮进行测试" << std::endl;
    
    while (1) {
        // 1. 驱动 LVGL (UI刷新、动画、输入响应)
        uint32_t time_till_next = lv_timer_handler();

        // 2. 线程休眠
        if (time_till_next > 5) time_till_next = 5;
        usleep(time_till_next * 1000); 

        // 3. [关键] 告诉 LVGL 过了 5ms (心跳)
        // 如果没有这一行，UI 里的定时器永远不会触发，画面会静止！
        lv_tick_inc(time_till_next); // 传入实际流逝的时间
    }

    // 程序退出前清理
    data_close();

    return 0;
}
