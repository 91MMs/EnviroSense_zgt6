/**
 ******************************************************************************
 * @file    ui_screen_devices_details.h
 * @brief   设备详细设置页面头文件
 * @details 提供 RGB LED、蜂鸣器、电机的详细配置界面
 * @author  EnviroSense Team
 * @date    2025
 ******************************************************************************
 */

#ifndef __UI_SCREEN_DEVICES_DETAILS_H
#define __UI_SCREEN_DEVICES_DETAILS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设备类型枚举（用于区分详情页类型）
 */
typedef enum {
    DEVICE_TYPE_RGBLED = 0,  /**< RGB LED 设备 */
    DEVICE_TYPE_BUZZER,      /**< 蜂鸣器设备 */
    DEVICE_TYPE_MOTOR        /**< 电机设备 */
} DeviceType_t;

/**
 * @brief 初始化设备详细设置页面
 * @param parent: 父容器对象
 * @param device_type: 要显示的设备类型
 * @note  根据 device_type 显示不同的设置界面
 */
void ui_screen_devices_details_init(lv_obj_t* parent, DeviceType_t device_type);

/**
 * @brief 反初始化设备详细设置页面
 * @note  清理定时器和动画资源
 */
void ui_screen_devices_details_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // __UI_SCREEN_DEVICES_DETAILS_H
