#ifndef __UI_SCREEN_DASHBOARD_H
#define __UI_SCREEN_DASHBOARD_H

#include "lvgl.h"

/**
 * @brief 初始化主控台屏幕的UI
 * @param parent 父对象 (通常是屏幕的根容器)
 */
void ui_screen_dashboard_init(lv_obj_t* parent);

/**
 * @brief 反初始化主控台屏幕的UI，释放资源
 */
void ui_screen_dashboard_deinit(void); 

#endif // __UI_SCREEN_DASHBOARD_H
