/**
 * @file sensor_app.c
 * @brief 传感器系统使用示例及集成指引
 * @author MmsY
 * @date 2025
*/

#include "sensor_app.h"
#include "main.h"
#include "sensor_task.h"
#include "gy30_sensor.h"
#include "sht30_sensor.h"
#include "mq2_sensor.h"
#include "i2c_bus_manager.h"
#include <stdio.h>
#include <string.h>

/* --------------------------- 调试配置 --------------------------- */
#define LOG_MODULE "SensorAPP"
#include "log.h"

/* --------------------------- 事件回调实现 --------------------------- */
void Sensor_EventCallback(const SensorEvent_t* event) {
    if (event == NULL) return;
    
    switch (event->event_type) {
        case SENSOR_EVENT_DATA_UPDATE: {
            // 数据更新事件
            switch (event->sensor_type) {
                case SENSOR_TYPE_GY30: {
                    // 获取光照强度
                    LOG_DEBUG("环境光照强度: %.1f lux", event->data.values.gy30.lux);
                    break;
                }
                case SENSOR_TYPE_SHT30: {
                    // 获取温湿度
                    LOG_DEBUG("环境温湿度: %.1f C, %.1f %%RH", event->data.values.sht30.temp, event->data.values.sht30.humi);
                    break;
                }
                case SENSOR_TYPE_SMOKE: {
                    // 获取烟雾浓度
                    LOG_INFO("环境烟雾浓度: %d PPM", event->data.values.smoke.ppm);
                    break;
                }
                default:
                    LOG_WARN("未知传感器数据更新事件");
                    break;
            }
            break;
        }
        case SENSOR_EVENT_STATUS_CHANGE: {
            // 获取传感器类型和状态
            const char* type_str = SensorType_ToString(event->sensor_type);
            const char* status_str = SensorStatus_ToString(event->status);

            LOG_DEBUG("传感器状态变化: 类型=%s, 新状态=%s", type_str, status_str);
            break;
        }
        case SENSOR_EVENT_ERROR: {
            // 错误事件
            LOG_ERROR("传感器错误: 类型=%s", SensorType_ToString(event->sensor_type));
            break;
        }
    }
}

/* --------------------------- 传感器系统初始化 --------------------------- */
void Sensor_System_Init(void) {
    bool success = false;
    LOG_INFO("传感器系统初始化开始...");
    do {
        // 1. 初始化传感器任务管理系统
        if (!SensorTask_Init())  break;

        // 2. 注册事件回调函数
        SensorTask_RegisterEventCallback(Sensor_EventCallback);

        // 3. 等待I2C总线稳定
        LOG_INFO("初始化I2C总线互斥锁...");
        if (!I2C_Bus_Manager_Init()) break;
        LOG_INFO("等待I2C总线稳定...");
        osDelay(500);

        // // 4. 注册GY30驱动
        if (!GY30_Sensor_Register())break;

        // // 5. 注册SHT30驱动
        if (!SHT30_Sensor_Register()) break;
        
        // 6. 注册MQ-2驱动
        if (!MQ2_Sensor_Register()) break;

        success = true;
    } while (0);
    
    if (success) LOG_INFO("传感器系统初始化完成!");
    else LOG_ERROR("传感器系统初始化失败!");
}
