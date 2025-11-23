/**
 * @file gy30_sensor.h
 * @brief GY30传感器集成到传感器任务系统头文件
 * @author MmsY
 * @date 2025
*/

#ifndef __GY30_SENSOR_H
#define __GY30_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* --------------------------- 公共函数声明 --------------------------- */
/**
 * @brief 注册GY30传感器到传感器任务系统
 * @return true: 成功, false: 失败
 */
bool GY30_Sensor_Register(void);

#ifdef  __cplusplus
}
#endif

#endif /* __GY30_SENSOR_H */
