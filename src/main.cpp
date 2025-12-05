#include <iostream>
#include <unistd.h> // for usleep
#include <vector>

// 1. 引入第三方库头文件 (验证头文件路径配置是否正确)
#include "lvgl.h"
#include <opencv2/opencv.hpp>
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
 * @brief Epic 2 测试: 数据层独立测试
 * 验证数据库初始化和图像存储功能
 */
void test_epic2_data_layer() {
    std::cout << "------------------------------------------" << std::endl;
    std::cout << ">>> [Test] Epic 2: 数据层独立测试" << std::endl;

    std::cout << ">>> 初始化数据库..." << std::endl;
    // 1. 测试初始化接口
    // 注意：data_init 会创建数据库连接，如果初始化成功，连接会保持打开状态
    if (data_init()) {
        std::cout << "[OK] 数据库连接成功" << std::endl;

        // 2. 创建一个测试图像 (纯黑色 100x100)
        cv::Mat test_img = cv::Mat::zeros(100, 100, CV_8UC3);
        
        std::cout << ">>> 尝试保存测试图像..." << std::endl;
        // 3. 测试保存接口
        if (data_saveImage(test_img)) {
            std::cout << "[OK] data_saveImage 测试通过！" << std::endl;
        } else {
            std::cerr << "[Failed] data_saveImage 测试失败！" << std::endl;
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

    std::cout << "BGR转灰度测试成功，灰度图尺寸: " << grayImage.cols << "x" << grayImage.rows << "，通道数: " << grayImage.channels() << std::endl;

    cv::Mat grayCopy = convertToGrayscale(grayImage);
    if (grayCopy.empty()) {
        std::cerr << "灰度图转灰度测试失败" << std::endl;
        return false;
    }// 测试灰度图像转换

    std::cout << "灰度图转灰度测试成功" << std::endl;
  
    std::cout << "=== 测试完成 ===" << std::endl;
    return true;
}

// ==========================================
// 主程序入口
// ==========================================
int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "   智能考勤系统 - 启动" << std::endl;
    std::cout << "==========================================" << std::endl;

    // ---------------------------------------------------------
    // 第一步：执行单元测试 (Epic 1 )
    // ---------------------------------------------------------
    test_epic1_framework();

    // ---------------------------------------------------------
    // 第二步：执行单元测试 (Epic 2 )
    // ---------------------------------------------------------
    test_epic2_data_layer();

    std::cout << "------------------------------------------" << std::endl;
    std::cout << ">>> 测试阶段结束，进入系统初始化..." << std::endl;

    // ---------------------------------------------------------
    // 第三步：执行单元测试 (Epic 3 )
    // ---------------------------------------------------------  
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
    // 第三步：系统正式初始化 (UI & 业务层)
    // ---------------------------------------------------------

    // A. 初始化 UI 层
    std::cout << ">>> 正在初始化 UI 层..." << std::endl;
    ui_init(); 
    std::cout << "[OK] UI 层初始化完成" << std::endl;

    // B. 初始化 业务层
    // 注意：data_init() 在 test_epic2_data_layer 中已经被调用过一次。
    // 如果 business_init 内部不依赖数据层重新初始化，这里没问题。
    // 如果业务层初始化逻辑是独立的，这里正常调用即可。
    std::cout << ">>> 正在初始化 业务层..." << std::endl;
    if (!business_init()) {
        std::cerr << "[Warning] 业务层初始化遇到问题 (如摄像头未连接)，但在模拟环境中继续运行..." << std::endl;
    } 
    else {
        std::cout << "[OK] 业务层初始化完成" << std::endl;
    } //确保业务层初始化成功

    // ---------------------------------------------------------
    // 第四步：进入主循环 (The Super Loop)
    // ---------------------------------------------------------
        std::cout << ">>> 系统主循环启动" << std::endl;
    
    while (1) {
        // 1. 驱动 LVGL (UI刷新、动画、输入响应)
        uint32_t time_till_next = lv_timer_handler();

        // 2. 驱动 业务逻辑 (人脸检测、识别)
        business_run_once();

        // 3. 线程休眠
        if (time_till_next > 5) time_till_next = 5;
        usleep(time_till_next * 1000); 
    }

    // 程序退出前清理
    data_close();

    return 0;
}
