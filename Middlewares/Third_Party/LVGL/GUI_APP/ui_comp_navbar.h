#ifndef __UI_COMP_NAVBAR_H
#define __UI_COMP_NAVBAR_H

#include "lvgl.h"
#include "ui_manager.h" // 需要引入ui_manager来获取 ui_screen_t 枚举

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建一个通用的底部导航栏组件
 * @param parent    要将导航栏创建于其上的父对象
 * @param active_screen 当前处于哪个屏幕，用于高亮对应的按钮
 * @return lv_obj_t* 创建的导航栏对象的指针
 */
lv_obj_t* ui_comp_navbar_create(lv_obj_t* parent, ui_screen_t active_screen);

#ifdef __cplusplus
}
#endif

#endif // __UI_COMP_NAVBAR_H
