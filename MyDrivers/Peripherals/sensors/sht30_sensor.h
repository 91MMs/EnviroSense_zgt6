/**
 * @file sht30_sensor.h
 * @brief SHT30传感器集成到传感器任务系统头文件
 * @author MmsY
 * @date 2025
*/

#ifndef __SHT30_SENSOR_H
#define __SHT30_SENSOR_H

#include "sensor_task.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif


/* --------------------------- 公共函数声明 --------------------------- */

/** 
 * @brief 注册SHT30传感器到传感器任务系统
 * @return true: 成功, false: 失败
 */
bool SHT30_Sensor_Register(void);


#ifdef  __cplusplus
}
#endif

#endif /* __SHT30_SENSOR_H */
