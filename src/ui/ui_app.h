#ifndef UI_APP_H
#define UI_APP_H

#include<stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(void);//初始化UI系统

// 全局退出标志，供 main.cpp 检查
extern volatile bool g_program_should_exit;

#ifdef __cplusplus
}
#endif

#endif