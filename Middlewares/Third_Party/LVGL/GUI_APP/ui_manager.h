#ifndef __UI_MANAGER_H
#define __UI_MANAGER_H

#include "lvgl.h"
#include "sensor_task.h"
#include "ui_screen_devices_details.h"

/**
 * @brief UI屏幕枚举
 */
typedef enum {
    UI_SCREEN_NONE = 0,
    UI_SCREEN_BOOT,
    UI_SCREEN_LOGIN,
    UI_SCREEN_DASHBOARD,
    UI_SCREEN_SENSORS_DETAILS,       // [CHANGED] 重命名
    UI_SCREEN_SENSORS_LISTS,         // [CHANGED] 重命名
    UI_SCREEN_DEVICE_DETAILS,
    UI_SCREEN_SETTINGS               // [ADD] 预留设置页面
} ui_screen_t;

/* 初始化UI系统 */
void ui_init(void);

/* 加载指定屏幕 */
void ui_load_screen(ui_screen_t screen);

/* 返回上一个屏幕 */
void ui_load_previous_screen(void);

/* 传感器类型相关 */
void ui_set_active_sensor(SensorType_t type);
SensorType_t ui_get_active_sensor(void);

/* 设备类型相关 */
void ui_set_active_device(DeviceType_t type);
DeviceType_t ui_get_active_device(void);

#endif // __UI_MANAGER_H
