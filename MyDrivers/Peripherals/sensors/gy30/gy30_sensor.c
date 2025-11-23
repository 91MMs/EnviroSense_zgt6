/**
 * @file gy30_sensor.c
 * @brief GY30传感器集成到传感器任务系统
 * @author MmsY
 * @date 2025
*/

#include "gy30_sensor.h"
#include "sensor_task.h"
#include "gy30.h"
#include <stdio.h>
#include <string.h>

/* --------------------------- 调试宏定义 --------------------------- */
// 这个log.h是我自己实现的日志系统，移植的时候必须拷贝上，否则得重新实现LOG__xxx等实现
#define LOG_MODULE "GY30_SENSOR"
#include "log.h"


/* --------------------------- 私有变量 --------------------------- */
static GY30_Device_t g_gy30_device;      // GY30设备实例

/* --------------------------- 私有函数声明 --------------------------- */
static bool GY30_Sensor_Init(SensorInstance_t* sensor);
static bool GY30_Sensor_Read(SensorInstance_t* sensor);
static bool GY30_Sensor_Deinit(SensorInstance_t* sensor);
static const char* GY30_Sensor_GetUnit(void);

/* --------------------------- 回调函数结构体 --------------------------- */
static const SensorCallbacks_t gy30_callbacks = {
    .init_func = GY30_Sensor_Init,
    .read_func = GY30_Sensor_Read,
    .deinit_func = GY30_Sensor_Deinit,
    .get_unit = GY30_Sensor_GetUnit
};

/* --------------------------- 公共函数实现 --------------------------- */

/**
 * @brief 注册GY30传感器到传感器任务系统
 */
bool GY30_Sensor_Register(void) {
    // 注册GY30传感器
    bool result = SensorTask_RegisterSensor(
        SENSOR_TYPE_GY30,           // 传感器类型
        "GY30 光照传感器",          // 传感器名称
        &gy30_callbacks,            // 回调函数
        &g_gy30_device,             // 设备句柄
        2000                        // 更新间隔2秒
    );

    return result;
}


/* --------------------------- 传感器回调函数实现 --------------------------- */

/**
 * @brief GY30传感器初始化回调
 */
static bool GY30_Sensor_Init(SensorInstance_t* sensor) {
    GY30_Device_t* device = (GY30_Device_t*)sensor->device_handle;

    // 已经初始化则直接返回
    if (device->is_initialized) {
        return true;
    }
    
    // 初始化GY30传感器
    GY30_Status_t status = GY30_Init(device, BH1750_DEFAULT_ADDR);
    if (status != GY30_OK) {
        LOG_ERROR("GY30传感器硬件初始化失败 (状态码: %d)", status);
        return false;
    }
    return true;
}

/**
 * @brief GY30传感器读取回调
 */
static bool GY30_Sensor_Read(SensorInstance_t* sensor) {
    GY30_Device_t* device = (GY30_Device_t*)sensor->device_handle;
    float lux_value;

    GY30_Status_t status = GY30_ReadLux(device, &lux_value);
    
    if (status == GY30_OK) {
        // 更新传感器数据
        sensor->data.values.gy30.lux = lux_value;
        return true;
    } else {
        // 读取失败
        LOG_ERROR("读取GY30传感器数据失败 (状态码: %d)", status);
        return false;
    }
}

/**
 * @brief GY30传感器反初始化回调
 */
static bool GY30_Sensor_Deinit(SensorInstance_t* sensor) {
    GY30_Device_t* device = (GY30_Device_t*)sensor->device_handle;
    
    if (device->is_initialized) {
        device->is_initialized = false;
        if (GY30_Sleep(device) != GY30_OK) {
            LOG_WARN("GY30传感器进入睡眠模式失败");
        }
        
    }

    return true;
}

/**
 * @brief 获取GY30传感器单位字符串
 */
static const char* GY30_Sensor_GetUnit(void) {
    return "lux";
}
