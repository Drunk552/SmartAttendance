cmake_minimum_required(VERSION 3.16)
project(SmartAttendance_Phase01 LANGUAGES C CXX)

# ==========================================
# 1. 编译选项配置
# ==========================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# ==========================================
# 2. 查找依赖库
# ==========================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
find_package(Freetype REQUIRED)
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc videoio highgui objdetect face)
if(OpenCV_FOUND)
    message(STATUS "Found OpenCV: ${OpenCV_VERSION}")
else()
    message(FATAL_ERROR "OpenCV 4.x not found!")
endif()
find_package(SQLite3 REQUIRED)

# ==========================================
# 3. LVGL 配置
# ==========================================
add_subdirectory(libs/lvgl)
target_compile_definitions(lvgl PUBLIC
    LV_CONF_PATH="${CMAKE_SOURCE_DIR}/lv_conf.h"
)
target_include_directories(lvgl PUBLIC
    ${SDL2_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
)
target_link_libraries(lvgl PUBLIC
    ${SDL2_LIBRARIES}
    Freetype::Freetype
)

# ==========================================
# 4. 主程序配置
# ==========================================
# 第一步：定义源文件列表（必须先定义！）
set(SRC_FILES
    src/main.cpp
    src/ui/ui_app.c
    src/business/face_demo.cpp
)

# 第二步：创建可执行文件（移到SRC_FILES之后）
add_executable(attendance_app ${SRC_FILES})

# 第三步：强制ui_app.c用C++编译（取消缩进！）
set_source_files_properties(src/ui/ui_app.c PROPERTIES LANGUAGE CXX)

# 第四步：包含头文件
target_include_directories(attendance_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/lvgl/src/libs/freetype
    ${CMAKE_SOURCE_DIR}/libs/lvgl/src
    ${SDL2_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
)

# 第五步：链接库（删掉多余的lvgl::examples/demos）
target_link_libraries(attendance_app PRIVATE
    lvgl
    ${OpenCV_LIBS}
    ${SDL2_LIBRARIES}
    ${SQLite3_LIBRARIES}
    ${FREETYPE_LIBRARIES}
)

# 编译信息输出
message(STATUS "Build Setup:")
message(STATUS " OpenCV Dir: ${OpenCV_DIR}")
message(STATUS " SQLite3 Lib: ${SQLite3_LIBRARIES}")
message(STATUS " LVGL Conf: ${CMAKE_SOURCE_DIR}/lv_conf.h")