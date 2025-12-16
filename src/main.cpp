/**
 * @file main.cpp
 * @brief 智能考勤系统主程序入口
 * @details 包含系统初始化、单元测试路由以及主循环
 * @version 1.3 (Fix Compilation Error)
 */

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h> // for usleep
#include <vector>
#include <cstdio>

// 1. 引入第三方库头文件
#include "lvgl.h"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp> 
#include <opencv2/highgui.hpp>   
#include <sqlite3.h>

// 2. 引入项目模块头文件
#include "ui/ui_app.h"          // UI层
#include "business/face_demo.h" // 业务层
#include "data/db_storage.h"    // 数据层

// ==========================================
// 测试函数定义区域
// ==========================================

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

/**
 * @brief Epic 2.2 & 2.3 联合测试
 * @details 验证数据播种(Seeding)是否生效，以及 DAO 接口的增删改查。
 */
void test_epic2_dao_and_seeding() {
    std::cout << "------------------------------------------" << std::endl;
    std::cout << ">>> [Test] Epic 2: DAO & Seeding 测试" << std::endl;

    // [修复] 1. 在函数顶部统一声明变量，避免重复声明错误
    std::vector<DeptInfo> depts;
    
    // --- 步骤 A: 验证数据播种 (Epic 2.3) ---
    std::cout << ">>> [1] 检查默认数据 (Seeding)..." << std::endl;
    
    depts = db_get_departments(); // 第一次获取
    if (!depts.empty()) {
        std::cout << "[OK] 默认部门已存在: " << depts[0].name << " (Seeding Success)" << std::endl;
    } else {
        std::cerr << "[Failed] 默认部门未生成！" << std::endl;
    }

    // 检查默认班次
    auto shifts = db_get_shifts();
    if (!shifts.empty()) {
        std::cout << "[OK] 默认班次已存在: " << shifts[0].name << " (" 
                  << shifts[0].start_time << "-" << shifts[0].end_time << ")" << std::endl;
    }

    // --- 步骤 B: 验证手动业务操作 (Epic 2.2) ---
    std::cout << ">>> [2] 测试手动添加部门..." << std::endl;

    // 尝试添加新部门
    if (db_add_department("R&D Center")) {
        depts = db_get_departments(); // [修复] 这里直接赋值刷新，不加 auto
        if (!depts.empty() && depts.back().name == "R&D Center") {
            std::cout << "[OK] 部门添加成功: R&D Center (ID: " << depts.back().id << ")" << std::endl;
        }
    } else {
        std::cerr << "[Warn] 部门添加跳过 (可能已存在)" << std::endl;
        // 确保 depts 是最新的
        depts = db_get_departments();
    }

    // --- 步骤 C: 注册管理员与考勤 (Epic 2.2) ---
    std::cout << ">>> [3] 测试用户注册与考勤..." << std::endl;

    cv::Mat dummy_face = cv::Mat::zeros(64, 64, CV_8UC1);
    
    UserData admin;
    admin.name = "Test_Admin";
    admin.password = "123456";
    admin.card_id = "CARD_888";
    admin.role = 1; // Administrator
    
    // 关联部门：如果有 R&D 就用 R&D，否则用默认的第一个
    admin.dept_id = 0;
    if (!depts.empty()) {
        admin.dept_id = depts.back().id; // 使用最后一个部门 (R&D)
    }

    int uid = db_add_user(admin, dummy_face);
    if (uid > 0) {
        // 验证读取
        UserData stored = db_get_user_info(uid);
        if (stored.password == "123456" && stored.role == 1) {
            std::cout << "[OK] 管理员注册并回读成功 (ID=" << uid << ")" << std::endl;
        } else {
            std::cerr << "[Failed] 用户信息回读不匹配" << std::endl;
        }

        // 测试考勤记录
        // status 1 = Late, shift_id = 0
        if (db_log_attendance(uid, 0, dummy_face, 1)) { 
            auto records = db_get_records(0, 9999999999LL);
            if (!records.empty() && records[0].user_name == "Test_Admin") {
                std::cout << "[OK] 考勤记录查询成功 (User: " << records[0].user_name 
                          << ", Dept: " << records[0].dept_name << ")" << std::endl;
            } else {
                std::cerr << "[Failed] 考勤记录未查到" << std::endl;
            }
        }
    } else {
        std::cerr << "[Failed] 用户注册失败" << std::endl;
    }
}

// ==========================================
// 主程序入口
// ==========================================
int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "   智能考勤系统 v1.2 - Phase 02" << std::endl;
    std::cout << "==========================================" << std::endl;

    // 1. 基础环境检查
    test_epic1_framework();

    // 2. 初始化数据层 (Phase 2 新 Schema + 自动播种)
    std::cout << ">>> 初始化数据层..." << std::endl;
    // 如果是第一次运行，建议先: rm attendance.db
    if (!data_init()) {
        std::cerr << "[Fatal] 数据库初始化失败，程序退出。" << std::endl;
        return -1;
    }

    // 3. 执行 Phase 2 核心测试 (DAO + Seeding)
    test_epic2_dao_and_seeding();

    // 4. 初始化业务层
    std::cout << ">>> 初始化业务层..." << std::endl;
    if (!business_init()) {
        std::cerr << "[Error] 业务层初始化失败。" << std::endl;
        data_close();
        return -1;
    }

    // 5. 初始化 UI 层
    std::cout << ">>> 初始化 UI 层..." << std::endl;
    ui_init(); 
    std::cout << "[OK] UI 层初始化完成" << std::endl;

    // 6. 进入主循环
    std::cout << ">>> 系统主循环启动" << std::endl;
    
    while (1) {
        // 驱动 LVGL 心跳
        uint32_t time_till_next = lv_timer_handler();
        if (time_till_next > 5) time_till_next = 5;
        usleep(time_till_next * 1000); 
        lv_tick_inc(time_till_next);
    }

    data_close();
    return 0;
}