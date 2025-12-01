#ifndef LV_CONF_H
#define LV_CONF_H

/*================
 * 基础配置
 *================*/

/* 颜色深度 */
// #define LV_COLOR_DEPTH 32

// /* 使用系统 tick（SDL 模拟足够） */
// #define LV_TICK_CUSTOM 0

// /*================
//  * 日志/断言（开发阶段建议打开）
//  *================*/
// #define LV_USE_LOG 1
// #define LV_LOG_LEVEL LV_LOG_LEVEL_INFO
#define LV_USE_LOG      1   // 开启日志，方便调试
#define LV_LOG_LEVEL    4  // 设置为 DEBUG 级别
/*================
 * 驱动：SDL 窗口 + 输入
 *================*/
#define LV_USE_SDL 1
/* 如果你的系统头是 <SDL.h>，改为 "SDL.h" */
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
// #define CUSTOM_FONT 1

/* 其他可选库：如 FreeType/PNG/JPEG 等，按需开启 */
#define LV_USE_FREETYPE 1
#define LV_USE_CACHE    1
#define LV_CACHE_DEF_SIZE (32 * 1024)
#endif /* LV_CONF_H */
