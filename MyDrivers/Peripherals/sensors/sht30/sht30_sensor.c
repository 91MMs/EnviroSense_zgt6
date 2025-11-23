/**
 * @file sht30_sensor.c
 * @brief SHT30传感器集成到传感器任务系统
 * @author MmsY
 * @date 2025
*/

#include "sht30_sensor.h"
#include "sht30.h"
#include <stdio.h>
#include <string.h>

/* --------------------------- 调试宏定义 --------------------------- */
#define LOG_MODULE "SHT30_SENSOR"
#include "log.h"


/* --------------------------- 私有变量 --------------------------- */
static SHT30_Device_t g_sht30_device; // SHT30设备实例

/* --------------------------- 私有函数声明 --------------------------- */
static bool SHT30_Sensor_Init(SensorInstance_t* sensor);
static bool SHT30_Sensor_Read(SensorInstance_t* sensor);
static bool SHT30_Sensor_Deinit(SensorInstance_t* sensor);
static const char* SHT30_Sensor_GetUnit(void);

/* --------------------------- 回调函数结构体 --------------------------- */
static const SensorCallbacks_t sht30_callbacks = {
    .init_func = SHT30_Sensor_Init,
    .read_func = SHT30_Sensor_Read,
    .deinit_func = SHT30_Sensor_Deinit,
    .get_unit = SHT30_Sensor_GetUnit
};

/* --------------------------- 公共函数实现 --------------------------- */

/**
 * @brief 注册SHT30传感器到传感器任务系统
 */
bool SHT30_Sensor_Register(void) {    
    // 1. 注册温度传感器
    bool result = SensorTask_RegisterSensor(
        SENSOR_TYPE_SHT30,      // 传感器类型    
        "SHT30 温湿度传感器",   // 传感器名称
        &sht30_callbacks,  // 回调函数
        &g_sht30_device,        // 设备句柄
        3000                    // 更新间隔3秒
    );

    return result;
}

/* --------------------------- 传感器回调函数实现 --------------------------- */

/**
 * @brief SHT30传感器初始化回调
 */
static bool SHT30_Sensor_Init(SensorInstance_t* sensor) {
    SHT30_Device_t* device = (SHT30_Device_t*)sensor->device_handle;
    
    // 已经初始化则直接返回
    if (device->is_initialized) {
        return true;
    }

    // 初始化SHT30设备
    SHT30_Status_t status = SHT30_Init(device, SHT30_DEFAULT_ADDR);
    if (status != SHT30_OK) {
        LOG_ERROR("SHT30传感器硬件初始化失败(状态码: %d)", status);
        return false;
    }
    return true;
}

/**
 * @brief SHT30传感器读取回调
 */
static bool SHT30_Sensor_Read(SensorInstance_t* sensor) {
    SHT30_Device_t* device = (SHT30_Device_t*)sensor->device_handle;
    float temp, humi;
    SHT30_Status_t status = SHT30_ReadTempHumi(device, &temp, &humi);
 
    if (status == SHT30_OK) {
        sensor->data.values.sht30.temp = temp;
        sensor->data.values.sht30.humi = humi;
        sensor->data.is_valid = true;
        return true;
    } else {
        sensor->data.is_valid = false;
        LOG_ERROR("读取SHT30传感器数据失败 (状态码: %d)", status);
        return false;
    }
}

/**
 * @brief SHT30传感器反初始化回调
 * @note  SHT30 本身就会处于低功耗模式，这里只是复位设备并标记为未初始化
 */
static bool SHT30_Sensor_Deinit(SensorInstance_t* sensor) {
    SHT30_Device_t* device = (SHT30_Device_t*)sensor->device_handle;

    if (device->is_initialized) {
        device->is_initialized = false;     // 标记为未初始化
        if (SHT30_Reset(device) != SHT30_OK) {
            LOG_WARN("SHT30传感器进入睡眠模式(复位)失败");
            return false;
        }
    }
    return true;
}

/**
 * @brief 获取SHT30传感器温度单位
 */
static const char* SHT30_Sensor_GetUnit(void) {
    // 对于多值传感器，这个函数意义不大，可以返回一个通用描述或空字符串
    return "C/%RH";
}
