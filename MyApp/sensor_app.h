/**
 ******************************************************************************
 * @file    sensor_app.h
 * @brief   传感器应用层头文件
 * @details 定义了整个传感器子系统的应用级接口。
 *          主程序(main.c)通过调用这里的函数来启动和控制传感器相关的功能。
 * @author  MmsY
 * @time    2025/11/23
 ******************************************************************************
 */

#ifndef __SENSOR_APP_H
#define __SENSOR_APP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void Sensor_System_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_APP_H */
