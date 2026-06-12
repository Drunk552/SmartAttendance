cmake_minimum_required(VERSION 3.16)
project(SmartAttendance_Phase01 LANGUAGES C CXX)

# ==========================================
# 1. 编译选项配置
# ==========================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 开启调试符号
set(CMAKE_BUILD_TYPE Debug)
# [关键] 生成 compile_commands.json，让 VS Code 自动识别头文件路径
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==========================================
# 2. 查找依赖库
# ==========================================
# 2.1 基础工具
find_package(PkgConfig REQUIRED)

# [Epic 4.4 新增] 查找线程库
find_package(Threads REQUIRED)  

# 2.2 LVGL 依赖 (SDL2, Freetype)
pkg_check_modules(SDL2 REQUIRED sdl2)
find_package(Freetype REQUIRED)

# 2.3 OpenCV 依赖 (包含 face 模块)
# 注意：OpenCV 4 通常安装在 /usr/include/opencv4，find_package 会自动处理
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc videoio highgui objdetect face imgcodecs)

# 2.4 SQLite3 依赖
find_package(SQLite3 REQUIRED) 
# 如果 find_package 找不到，可以使用: pkg_check_modules(SQLITE3 REQUIRED sqlite3)

# 2.5 xlsxwriter 依赖
pkg_check_modules(XLSXWRITER REQUIRED xlsxwriter)

# ==========================================
# 3. 第三方库配置 (LVGL)
# ==========================================

# [新增] 集成 libxlsxwriter (用于生成 Excel 报表)
#include(FetchContent)
#FetchContent_Declare(
#    libxlsxwriter
#    GIT_REPOSITORY https://github.com/jmcnamara/libxlsxwriter.git
#    GIT_TAG        RELEASE_1.1.5  # 使用稳定版本
#)
#FetchContent_MakeAvailable(libxlsxwriter)

# 设置变量（注意：这一行必须放在 add_subdirectory 之前！）
# 这样 LVGL 的构建脚本在运行时就能知道配置文件在哪里。
set(LV_CONF_PATH "${CMAKE_SOURCE_DIR}/lv_conf.h")

# 引入 libs/lvgl 子目录
add_subdirectory(libs/lvgl)

# 定义 LV_CONF_PATH 宏，指向根目录的配置文件
# 注意：这里必须是绝对路径或相对于构建目录的正确路径
target_compile_definitions(lvgl PUBLIC LV_CONF_PATH="${CMAKE_SOURCE_DIR}/lv_conf.h")

# 配置 LVGL 的头文件和链接库
target_include_directories(lvgl PUBLIC 
    ${SDL2_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
)
target_link_libraries(lvgl PUBLIC 
    ${SDL2_LIBRARIES}
    Freetype::Freetype
)

# 检查 db_storage.h 是否存在
if(EXISTS "${CMAKE_SOURCE_DIR}/src/data/db_storage.h")
    message(STATUS "[SUCCESS] db_storage.h found: ${CMAKE_SOURCE_DIR}/src/data/db_storage.h")
else()
    message(FATAL_ERROR "[ERROR] db_storage.h NOT found! Check path: ${CMAKE_SOURCE_DIR}/src/data/")
endif()

# ==========================================
# 4. 主程序配置
# ==========================================
# 自动查找 src 下的源文件 (实际开发建议显式列出以避免遗漏)
file(GLOB_RECURSE SRC_FILES
    "src/main.cpp"
    
    # UI 层 - 显式列出需要的文件
    "src/ui/common/ui_style.cpp"
    "src/ui/common/ui_widgets.cpp"
    "src/ui/common/ime_chinese.cpp"
    "src/ui/common/t9_keyboard.cpp"
    "src/ui/common/t9_keymap.cpp"
    "src/ui/common/matrix_t9_keyboard.cpp"
    "src/ui/porting/hal_keypad.cpp"
    "src/ui/managers/*.cpp"
    "src/ui/screens/home/*.cpp"
    "src/ui/screens/menu/*.cpp"
    "src/ui/screens/user_mgmt/*.cpp"
    "src/ui/screens/record_query/*.cpp"
    "src/ui/screens/att_stats/*.cpp"
    "src/ui/screens/att_design/*.cpp"
    "src/ui/screens/system/*.cpp"
    "src/ui/screens/sys_info/*.cpp"
    "src/ui/ui_app.cpp"
    "src/ui/ui_controller.cpp"
    "src/ui/*.c"

    # 业务层
    "src/business/event_bus.cpp"
    "src/business/*.cpp"

    # 数据层
    "src/data/db_storage.cpp"  
)

add_executable(attendance_app ${SRC_FILES})

# 包含头文件路径
target_include_directories(attendance_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/lvgl
    ${CMAKE_SOURCE_DIR}/src/business
    ${CMAKE_SOURCE_DIR}/src/data
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/common
    ${CMAKE_SOURCE_DIR}/src/ui/managers
    ${CMAKE_SOURCE_DIR}/src/ui/screens
    # 添加 lv_freetype.h 所在的目录
    ${CMAKE_SOURCE_DIR}/libs/lvgl/src/libs/freetype

     #添加db_storage.h
    ${PROJECT_SOURCE_DIR}/src/data 
    ${PROJECT_SOURCE_DIR}/src/business

    ${SDL2_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    "/usr/include/opencv4"   # [强制] Linux/WSL OpenCV4 标准路径
    ${XLSXWRITER_INCLUDE_DIRS}
)

# 链接所有库
target_link_libraries(attendance_app PRIVATE
    lvgl                # UI 库
    ${OpenCV_LIBS}      # 图像处理库
    SQLite::SQLite3     # 数据库
    ${SDL2_LIBRARIES}   # 驱动支持
    ${XLSXWRITER_LIBRARIES}  # 链接 libxlsxwriter
    Threads::Threads  # 链接线程库
    # Deleted: wiringPi
)

# 树莓派硬件构建时才链接 wiringPi 并定义 RPI_BUILD 宏
if(RPI_BUILD)
    target_link_libraries(attendance_app PRIVATE wiringPi)
    target_compile_definitions(attendance_app PRIVATE RPI_BUILD)
    message(STATUS "RPI_BUILD=ON: 链接 wiringPi，启用 GPIO 矩阵键盘")
else()
    message(STATUS "RPI_BUILD=OFF: 跳过 wiringPi，使用 SDL 模拟模式")
endif()

# 编译信息输出
message(STATUS "Build Setup:")
message(STATUS "  OpenCV Dir: ${OpenCV_DIR}")
message(STATUS "  SQLite3 Lib: ${SQLite3_LIBRARIES}")
message(STATUS "  LVGL Conf: ${CMAKE_SOURCE_DIR}/lv_conf.h")
message(STATUS "Compile Commands Export: ${CMAKE_EXPORT_COMPILE_COMMANDS}")

# ==========================================
# 5. 测试目标配置
# ==========================================
message(STATUS "添加稳定性测试支持...")

# 创建测试目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_reports)

# 添加自定义目标：运行稳定性测试
add_custom_target(stability_test
    COMMAND chmod +x ${CMAKE_SOURCE_DIR}/tools/stability_test.sh
    COMMAND cd ${CMAKE_BINARY_DIR} && ${CMAKE_SOURCE_DIR}/tools/stability_test.sh
    COMMENT "运行1小时稳定性测试 (内存监控 + 崩溃检测)"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 添加自定义目标：运行快速稳定性测试
add_custom_target(quick_stability_test
    COMMAND chmod +x ${CMAKE_SOURCE_DIR}/tools/quick_stability_test.sh
    COMMAND cd ${CMAKE_BINARY_DIR} && ${CMAKE_SOURCE_DIR}/tools/quick_stability_test.sh
    COMMENT "运行10分钟快速稳定性测试"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 添加自定义目标：分析测试结果
add_custom_target(analyze_stability
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/analyze_stability.py ${CMAKE_BINARY_DIR}/memory_usage.csv ${CMAKE_BINARY_DIR}/test_reports
    COMMENT "分析稳定性测试结果并生成报告"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS attendance_app
)

# 添加自定义目标：运行完整测试套件
add_custom_target(test_all
    COMMAND echo "开始完整测试套件..."
    COMMAND make quick_stability_test || true
    COMMAND echo "快速测试完成，开始完整稳定性测试..."
    COMMAND make stability_test || true
    COMMAND echo "稳定性测试完成，开始分析结果..."
    COMMAND make analyze_stability || true
    COMMAND echo "测试套件执行完成！"
    COMMENT "执行完整测试套件 (快速测试 + 完整测试 + 分析)"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS attendance_app
)

message(STATUS "测试目标已添加:")
message(STATUS "  make stability_test      - 运行1小时稳定性测试")
message(STATUS "  make quick_stability_test - 运行10分钟快速测试")
message(STATUS "  make analyze_stability   - 分析测试结果")
message(STATUS "  make test_all            - 运行完整测试套件")