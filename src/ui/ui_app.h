#ifndef UI_APP_H
#define UI_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UI 子系统入口初始化
 * @details 负责 HAL (SDL/FB) 初始化、输入设备配置、管理器启动以及加载主页
 */
void ui_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // UI_APP_H