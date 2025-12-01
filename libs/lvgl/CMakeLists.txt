cmake_minimum_required(VERSION 3.16)
project(lvgl_sdl_wsl_template_v9_thirdparty C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# FreeType
find_package(Freetype REQUIRED)

add_subdirectory(third_party/lvgl EXCLUDE_FROM_ALL)

add_executable(app
  src/main.c
)

# 让 lvgl 与 app 都使用同一份 lv_conf.h（注意：不要转义引号）
target_compile_definitions(lvgl PUBLIC LV_CONF_PATH="${CMAKE_SOURCE_DIR}/lv_conf.h")
target_compile_definitions(app  PRIVATE LV_CONF_PATH="${CMAKE_SOURCE_DIR}/lv_conf.h")

# 把 FreeType 链接到 lvgl（编译 lvgl/src/libs/freetype/*.c 需要）
target_link_libraries(lvgl PUBLIC Freetype::Freetype)
target_include_directories(lvgl PUBLIC ${FREETYPE_INCLUDE_DIRS})

# SDL 头文件目录给到 lvgl（编译 SDL 驱动）
target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})

# app 需要能包含 lvgl 的头，以及 lv_freetype.h（它在 lvgl 源树里）
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # 新增：和根目录CMake一致的路径（相对+绝对）
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl/src/libs/freetype
    /home/alice/work/SmartAttendance/libs/lvgl/third_party/lvgl/src/libs/freetype#新增——到这
    ${CMAKE_SOURCE_DIR}/third_party/lvgl
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/libs/freetype
    ${SDL2_INCLUDE_DIRS}
)

# 链接
target_link_libraries(app PRIVATE lvgl ${SDL2_LIBRARIES})

option(ENABLE_ASAN "Enable AddressSanitizer/UBSan" OFF)
if(ENABLE_ASAN)
  target_compile_options(app PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(app PRIVATE -fsanitize=address,undefined)
endif()

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(app PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# 可选：编译时打印下关键变量，便于确认
message(STATUS "LV_CONF_PATH  -> ${CMAKE_SOURCE_DIR}/lv_conf.h")
message(STATUS "FREETYPE_INC  -> ${FREETYPE_INCLUDE_DIRS}")
message(STATUS "SDL2_INC      -> ${SDL2_INCLUDE_DIRS}")
message(STATUS "FREETYPE_LIBRARIES -> ${FREETYPE_LIBRARIES}")
message(STATUS "SDL2_LIBRARIES -> ${SDL2_LIBRARIES}")
