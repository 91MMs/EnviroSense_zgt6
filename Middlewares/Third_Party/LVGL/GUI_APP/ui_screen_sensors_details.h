#ifndef __UI_SCREEN_SENSORS_DETAILS_H
#define __UI_SCREEN_SENSORS_DETAILS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化传感器详情页面的UI
 * @param parent 父对象 (通常是屏幕的根容器)
 */
void ui_screen_sensors_details_init(lv_obj_t *parent);

/**
 * @brief 反初始化传感器详情页面的UI，释放资源
 */
void ui_screen_sensors_details_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // __UI_SCREEN_SENSORS_DETAILS_H
