/**
 * @file mq2_sensor.h
 * @brief MQ-2传感器集成到传感器任务系统头文件
 * @author MmsY
 * @date 2025
*/

#ifndef __MQ2_SENSOR_H
#define __MQ2_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* --------------------------- 公共函数声明 --------------------------- */
/**
 * @brief 注册MQ-2传感器到传感器任务系统
 * @return true: 成功, false: 失败
 */
bool MQ2_Sensor_Register(void);

#ifdef  __cplusplus
}
#endif

#endif /* __MQ2_SENSOR_H */
