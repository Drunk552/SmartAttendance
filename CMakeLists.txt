cmake_minimum_required(VERSION 3.16)
project(SmartAttendance_Phase01 LANGUAGES C CXX)

# ==========================================
# 1. 编译选项配置
# ==========================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
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

include(CTest)

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
set(SA_PLATFORM "pc" CACHE STRING "SmartAttendance platform implementation")
set_property(CACHE SA_PLATFORM PROPERTY STRINGS pc raspberry_pi lowmem_linux)

if(SA_PLATFORM STREQUAL "pc")
    add_library(smartattendance_platform STATIC
        src/platform/pc/gstreamer_camera.cpp
        src/platform/pc/sdl_display.cpp
        src/platform/pc/sdl_keypad.cpp
        src/platform/pc/system_rtc.cpp
        src/platform/pc/path_storage.cpp
        src/platform/pc/platform_factory.cpp
    )
    target_compile_definitions(smartattendance_platform PUBLIC SA_PLATFORM_PC=1)
else()
    message(FATAL_ERROR
        "SA_PLATFORM=${SA_PLATFORM} is not implemented: target board/BSP facts are not frozen")
endif()
target_include_directories(smartattendance_platform PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(smartattendance_platform PUBLIC
    lvgl
    ${OpenCV_LIBS}
    Threads::Threads
)

add_library(smartattendance_biometric STATIC
    src/biometric/face/face_detector.cpp
    src/biometric/face/face_preprocessor.cpp
    src/biometric/face/face_recognizer.cpp
    src/biometric/face/face_model_manager.cpp
    src/biometric/face/face_recognition_engine.cpp
)
target_include_directories(smartattendance_biometric PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(smartattendance_biometric PUBLIC ${OpenCV_LIBS})

add_library(smartattendance_face_workers STATIC
    src/business/face_capture_worker.cpp
    src/business/face_punch_worker.cpp
    src/business/punch_request_queue.cpp
)
target_include_directories(smartattendance_face_workers PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(smartattendance_face_workers PUBLIC
    smartattendance_biometric
    ${OpenCV_LIBS}
    Threads::Threads
)

add_library(smartattendance_storage STATIC
    src/storage/database.cpp
    src/storage/sqlite/legacy_organization_dao.cpp
    src/storage/sqlite/legacy_employee_dao.cpp
    src/storage/sqlite/legacy_attendance_dao.cpp
    src/storage/sqlite/legacy_schedule_dao.cpp
    src/storage/sqlite/legacy_config_dao.cpp
)
target_include_directories(smartattendance_storage PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(smartattendance_storage PUBLIC
    SQLite::SQLite3
    ${OpenCV_LIBS}
    Threads::Threads
)

add_library(smartattendance_repositories STATIC
    src/storage/sqlite/legacy_employee_repository.cpp
    src/storage/sqlite/legacy_config_repository.cpp
    src/storage/sqlite/legacy_schedule_repository.cpp
    src/storage/sqlite/legacy_attendance_repository.cpp
)
target_include_directories(smartattendance_repositories PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(smartattendance_repositories PUBLIC
    smartattendance_storage
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(smartattendance_platform PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(smartattendance_biometric PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(smartattendance_face_workers PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(smartattendance_storage PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(smartattendance_repositories PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 自动查找 src 下的源文件 (实际开发建议显式列出以避免遗漏)
file(GLOB_RECURSE SRC_FILES
    "src/main.cpp"

    # 应用生命周期层
    "src/app/*.cpp"

    # 核心打卡规则与阶段三过渡服务
    "src/core/attendance/punch_rule.cpp"
    "src/services/employee_service.cpp"
    "src/services/punch_service.cpp"
    "src/ui/presenters/employee_lookup_presenter.cpp"
    
    # UI 层 - 显式列出需要的文件
    "src/ui/common/ui_style.cpp"
    "src/ui/common/ui_widgets.cpp"
    "src/ui/common/t9_keyboard.cpp"
    "src/ui/common/t9_keymap.cpp"
    "src/ui/common/matrix_t9_keyboard.cpp"
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
    "src/business/*.cpp"

)

# 阶段五新 Worker 使用显式 Target，避免继续依赖旧 business GLOB 自动接入。
list(REMOVE_ITEM SRC_FILES
    ${CMAKE_SOURCE_DIR}/src/business/face_capture_worker.cpp
    ${CMAKE_SOURCE_DIR}/src/business/face_punch_worker.cpp
    ${CMAKE_SOURCE_DIR}/src/business/punch_request_queue.cpp
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
    smartattendance_repositories
    smartattendance_face_workers
    smartattendance_biometric
    smartattendance_platform
    # Deleted: wiringPi
)

# 编译信息输出
message(STATUS "Build Setup:")
message(STATUS "  SA Platform: ${SA_PLATFORM}")
message(STATUS "  OpenCV Dir: ${OpenCV_DIR}")
message(STATUS "  SQLite3 Lib: ${SQLite3_LIBRARIES}")
message(STATUS "  LVGL Conf: ${CMAKE_SOURCE_DIR}/lv_conf.h")
message(STATUS "Compile Commands Export: ${CMAKE_EXPORT_COMPILE_COMMANDS}")

if(BUILD_TESTING)
    add_executable(application_lifecycle_test
        src/app/application.cpp
        src/app/application_services.cpp
        src/app/task_manager.cpp
        src/app/ui_background_job_queue.cpp
        src/app/ui_system_status_mailbox.cpp
        src/core/attendance/punch_rule.cpp
        src/services/employee_service.cpp
        src/services/punch_service.cpp
        src/ui/presenters/employee_lookup_presenter.cpp
        tests/unit/application_lifecycle_test.cpp
    )
    target_include_directories(application_lifecycle_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(application_lifecycle_test PRIVATE
        SQLite::SQLite3
        ${OpenCV_LIBS}
        Threads::Threads
        smartattendance_repositories
        smartattendance_biometric
    )
    add_test(
        NAME application_lifecycle
        COMMAND application_lifecycle_test
    )

    add_executable(application_services_test
        src/app/application_services.cpp
        src/core/attendance/punch_rule.cpp
        src/services/employee_service.cpp
        src/services/punch_service.cpp
        tests/unit/application_services_test.cpp
    )
    target_include_directories(application_services_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(application_services_test PRIVATE
        SQLite::SQLite3
        ${OpenCV_LIBS}
        Threads::Threads
        smartattendance_repositories
        smartattendance_biometric
    )
    add_test(
        NAME application_services
        COMMAND application_services_test
    )

    add_executable(task_manager_test
        src/app/task_manager.cpp
        src/app/ui_background_job_queue.cpp
        src/app/ui_system_status_mailbox.cpp
        tests/unit/task_manager_test.cpp
    )
    target_include_directories(task_manager_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(task_manager_test PRIVATE Threads::Threads)
    add_test(
        NAME task_manager
        COMMAND task_manager_test
    )

    add_executable(ui_background_job_queue_test
        src/app/ui_background_job_queue.cpp
        tests/unit/ui_background_job_queue_test.cpp
    )
    target_include_directories(ui_background_job_queue_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(ui_background_job_queue_test PRIVATE Threads::Threads)
    add_test(
        NAME ui_background_job_queue
        COMMAND ui_background_job_queue_test
    )

    add_executable(ui_system_status_mailbox_test
        src/app/ui_system_status_mailbox.cpp
        tests/unit/ui_system_status_mailbox_test.cpp
    )
    target_include_directories(ui_system_status_mailbox_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(ui_system_status_mailbox_test PRIVATE Threads::Threads)
    add_test(
        NAME ui_system_status_mailbox
        COMMAND ui_system_status_mailbox_test
    )

    add_executable(punch_service_test
        src/core/attendance/punch_rule.cpp
        src/services/punch_service.cpp
        tests/unit/punch_service_test.cpp
    )
    target_include_directories(punch_service_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(punch_service_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
    add_test(
        NAME punch_service
        COMMAND punch_service_test
    )

    add_executable(employee_service_test
        src/services/employee_service.cpp
        tests/unit/employee_service_test.cpp
    )
    target_include_directories(employee_service_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(employee_service_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
    add_test(
        NAME employee_service
        COMMAND employee_service_test
    )

    add_executable(employee_lookup_presenter_test
        src/services/employee_service.cpp
        src/ui/presenters/employee_lookup_presenter.cpp
        tests/unit/employee_lookup_presenter_test.cpp
    )
    target_include_directories(employee_lookup_presenter_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(employee_lookup_presenter_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
    add_test(
        NAME employee_lookup_presenter
        COMMAND employee_lookup_presenter_test
    )

    add_executable(attendance_rule_compat_test
        src/business/attendance_rule.cpp
        src/core/attendance/punch_rule.cpp
        tests/unit/attendance_rule_compat_test.cpp
    )
    target_include_directories(attendance_rule_compat_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(attendance_rule_compat_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
    add_test(
        NAME attendance_rule_compat
        COMMAND attendance_rule_compat_test
    )

    add_executable(punch_request_queue_test
        src/business/punch_request_queue.cpp
        tests/unit/punch_request_queue_test.cpp
    )
    target_include_directories(punch_request_queue_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(punch_request_queue_test PRIVATE Threads::Threads)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(punch_request_queue_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
    add_test(
        NAME punch_request_queue
        COMMAND punch_request_queue_test
    )

    add_executable(face_recognition_engine_test
        tests/unit/face_recognition_engine_test.cpp
    )
    target_include_directories(face_recognition_engine_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(face_recognition_engine_test PRIVATE
        smartattendance_biometric
        ${OpenCV_LIBS}
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(face_recognition_engine_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
    add_test(
        NAME face_recognition_engine
        COMMAND face_recognition_engine_test
    )

    add_executable(face_capture_worker_test
        tests/unit/face_capture_worker_test.cpp
    )
    target_include_directories(face_capture_worker_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(face_capture_worker_test PRIVATE
        smartattendance_face_workers
        smartattendance_biometric
        ${OpenCV_LIBS}
        Threads::Threads
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(face_capture_worker_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
    add_test(
        NAME face_capture_worker
        COMMAND face_capture_worker_test
    )

    add_executable(platform_factory_test
        tests/unit/platform_factory_test.cpp
    )
    target_include_directories(platform_factory_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(platform_factory_test PRIVATE
        smartattendance_platform
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(platform_factory_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
    add_test(
        NAME platform_factory
        COMMAND platform_factory_test
    )

    add_executable(legacy_punch_repositories_test
        src/core/attendance/punch_rule.cpp
        src/services/punch_service.cpp
        tests/integration/legacy_punch_repositories_test.cpp
    )
    target_include_directories(legacy_punch_repositories_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(legacy_punch_repositories_test PRIVATE
        SQLite::SQLite3
        ${OpenCV_LIBS}
        Threads::Threads
        smartattendance_repositories
    )
    add_test(
        NAME legacy_punch_repositories
        COMMAND legacy_punch_repositories_test
    )

    add_executable(legacy_employee_repository_test
        tests/integration/legacy_employee_repository_test.cpp
    )
    target_include_directories(legacy_employee_repository_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(legacy_employee_repository_test PRIVATE
        SQLite::SQLite3
        ${OpenCV_LIBS}
        Threads::Threads
        smartattendance_repositories
    )
    add_test(
        NAME legacy_employee_repository
        COMMAND legacy_employee_repository_test
    )

    add_executable(legacy_config_repository_test
        tests/integration/legacy_config_repository_test.cpp
    )
    target_include_directories(legacy_config_repository_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(legacy_config_repository_test PRIVATE
        SQLite::SQLite3
        ${OpenCV_LIBS}
        Threads::Threads
        smartattendance_repositories
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(legacy_config_repository_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
    add_test(
        NAME legacy_config_repository
        COMMAND legacy_config_repository_test
    )
endif()

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
