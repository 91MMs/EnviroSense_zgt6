/**
 * @file mq2_sensor.c
 * @brief MQ-2传感器集成到传感器任务系统
 * @author MmsY
 * @date 2025
*/

#include "mq2_sensor.h"
#include "sensor_task.h"
#include "mq2.h"
#include <stdio.h>
#include <string.h>

/* --------------------------- 调试宏定义 --------------------------- */
#define LOG_MODULE "MQ2_SENSOR"
#include "log.h"

/* --------------------------- 私有变量 --------------------------- */
static MQ2_Device_t g_mq2_device;      // MQ-2设备实例

/* --------------------------- 私有函数声明 --------------------------- */
static bool MQ2_Sensor_Init(SensorInstance_t* sensor);
static bool MQ2_Sensor_Read(SensorInstance_t* sensor);
static bool MQ2_Sensor_Deinit(SensorInstance_t* sensor);
static const char* MQ2_Sensor_GetUnit(void);

/* --------------------------- 回调函数结构体 --------------------------- */
static const SensorCallbacks_t mq2_callbacks = {
    .init_func = MQ2_Sensor_Init,
    .read_func = MQ2_Sensor_Read,
    .deinit_func = MQ2_Sensor_Deinit,
    .get_unit = MQ2_Sensor_GetUnit
};

/* --------------------------- 公共函数实现 --------------------------- */

/**
 * @brief 注册MQ-2传 感器到传感器任务系统
 */
bool MQ2_Sensor_Register(void) {
    // 注册MQ-2传感器
    bool result = SensorTask_RegisterSensor(
        SENSOR_TYPE_SMOKE,          // 传感器类型
        "MQ-2 烟雾传感器",          // 传感器名称
        &mq2_callbacks,             // 回调函数
        &g_mq2_device,              // 设备句柄
        2000                        // 更新间隔2秒
    );

    return result;
}

/* --------------------------- 传感器回调函数实现 --------------------------- */

/**
 * @brief MQ-2传感器初始化回调
 */
static bool MQ2_Sensor_Init(SensorInstance_t* sensor) {
    MQ2_Device_t* device = (MQ2_Device_t*)sensor->device_handle;

    // 已经初始化则直接返回
    if (device->is_initialized) {
        return true;
    }
    
    // 初始化MQ-2传感器
    MQ2_Status_t status = MQ2_Init(device);
    if (status != MQ2_OK) {
        LOG_ERROR("MQ-2传感器硬件初始化失败 (状态码: %d)", status);
        return false;
    }
    return true;
}

/**
 * @brief MQ-2传感器读取回调
 */
static bool MQ2_Sensor_Read(SensorInstance_t* sensor) {
    MQ2_Device_t* device = (MQ2_Device_t*)sensor->device_handle;
    int ppm_value;

    MQ2_Status_t status = MQ2_ReadPPM(device, &ppm_value);
    
    if (status == MQ2_OK) {
        // 更新传感器数据
        sensor->data.values.smoke.ppm = ppm_value;
        return true;
    } else {
        // 读取失败
        LOG_ERROR("读取MQ-2传感器数据失败 (状态码: %d)", status);
        return false;
    }
}

/**
 * @brief MQ-2传感器反初始化回调
 */
static bool MQ2_Sensor_Deinit(SensorInstance_t* sensor) {
    MQ2_Device_t* device = (MQ2_Device_t*)sensor->device_handle;
    
    if (device->is_initialized) {
        device->is_initialized = false;
        LOG_INFO("MQ-2传感器反初始化完成");
    }

    return true;
}

/**
 * @brief 获取MQ-2传感器单位字符串
 */
static const char* MQ2_Sensor_GetUnit(void) {
    return "PPM";
}
