#ifndef __UI_SCREEN_SENSORS_LISTS_H
#define __UI_SCREEN_SENSORS_LISTS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化传感器列表屏幕的UI
 * @param parent 父对象 (通常是屏幕的根容器)
 */
void ui_screen_sensors_lists_init(lv_obj_t* parent);

/**
 * @brief 反初始化传感器列表屏幕的UI，释放资源
 */
void ui_screen_sensors_lists_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // __UI_SCREEN_SENSORS_LISTS_H
