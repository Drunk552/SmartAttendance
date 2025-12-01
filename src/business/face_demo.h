// src/business/face_demo.h
#ifndef FACE_DEMO_H
#define FACE_DEMO_H

// 注意：是双下划线__cplusplus
#ifdef __cplusplus
extern "C" {
#endif

// 初始化函数
bool business_init();
// 单次业务函数
void business_run_once();

#ifdef __cplusplus
}
#endif

#endif // FACE_DEMO_H