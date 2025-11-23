/**
 * @file gy30.c
 * @brief GY-30 (BH1750) 光照传感器驱动源文件
 * @author MmsY
 * @date 2025
*/

#include "gy30.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
#include "i2c_bus_manager.h"

/* --------------------------- 日志配置 --------------------------- */
// 这个log.h是我自己实现的日志系统，移植的时候必须拷贝上，否则得重新实现LOG__xxx等实现
#define LOG_MODULE "GY30"
#include "log.h"

/* --------------------------- 私有函数声明 --------------------------- */
static GY30_Status_t GY30_WriteCommand(GY30_Device_t *device, uint8_t command);
static GY30_Status_t GY30_ReadData(GY30_Device_t *device, uint8_t *data, uint16_t size);
static uint32_t GY30_GetTickMs(void);
static GY30_Status_t GY30_ApplyMode(GY30_Device_t *device);
static GY30_Status_t GY30_ExecuteWithRetry(GY30_Device_t *device, 
                                        GY30_Status_t (*func)(GY30_Device_t *), 
                                        const char *action_name);

/* --------------------------- 公共函数实现 --------------------------- */

/**
 * @brief 初始化GY-30传感器
 */
GY30_Status_t GY30_Init(GY30_Device_t *device, uint8_t i2c_addr) {
    if (device == NULL) return GY30_ERROR;

    if (device->is_initialized) {
        LOG_DEBUG("GY30设备已经初始化，无需重复初始化");
        return GY30_OK;
    }

    LOG_INFO("开始初始化GY30设备, I2C地址: 0x%02X", i2c_addr);
    
    // 初始化设备结构体
    memset(device, 0, sizeof(GY30_Device_t));
    device->addr = i2c_addr;
    device->mode = GY30_MODE_HIGH_RES;
    device->is_initialized = true;  // 临时标记为已初始化
    device->last_read_time = 0;
    
    // 初始化设备
    if (GY30_ExecuteWithRetry(device, GY30_IsOnline, "查询设备在线状态") != GY30_OK) {
        return GY30_ERROR;
    }
    if (GY30_ExecuteWithRetry(device, GY30_Reset, "复位设备") != GY30_OK) {
        return GY30_ERROR;
    }
    if (GY30_ExecuteWithRetry(device, GY30_Wakeup, "设备唤醒") != GY30_OK) {
        return GY30_ERROR;
    }
    if (GY30_ExecuteWithRetry(device, GY30_ApplyMode, "设置工作模式") != GY30_OK) {
        device->is_initialized = false;  // 将GY30_ApplyMode()设置的临时标记清除
        return GY30_ERROR;
    }
    
    device->is_initialized = true;
    LOG_INFO("GY30设备初始化成功");
    return GY30_OK;
}

/**
 * @brief 读取光照强度值
 */
GY30_Status_t GY30_ReadLux(GY30_Device_t *device, float *lux) {
    if (device == NULL || lux == NULL || !device->is_initialized) {
        return GY30_ERROR;
    }

    // 对于单次模式，重新触发测量
    if (device->mode >= GY30_MODE_ONE_LOW_RES) {
        GY30_Status_t status = GY30_WriteCommand(device, (uint8_t)device->mode);
        if (status != GY30_OK) {
            return status;
        }
        device->last_read_time = GY30_GetTickMs();
    }
    // 对于“连续测量”模式，传感器会自动进行下一次测量，无需发送命令
    
    // 检查是否需要等待测量完成
    uint32_t required_time = GY30_GetMeasurementTime(device->mode); // 获取当前模式的测量时间
    osDelay(required_time); 
    
    // 读取2字节原始数据
    uint8_t data[2];
    GY30_Status_t status = GY30_ReadData(device, data, 2);
    if (status != GY30_OK) {
        return status;
    }
    
    // 转换为光照强度值
    uint16_t raw_value = (data[0] << 8) | data[1];
    
    // 根据模式计算lux值
    switch (device->mode) {
        case GY30_MODE_LOW_RES:
        case GY30_MODE_ONE_LOW_RES:
            *lux = (float)raw_value / 1.2f;  // 低分辨率模式
            break;
            
        case GY30_MODE_HIGH_RES:
        case GY30_MODE_ONE_HIGH_RES:
            *lux = (float)raw_value / 1.2f;  // 高分辨率模式
            break;
            
        case GY30_MODE_HIGH_RES2:
        case GY30_MODE_ONE_HIGH_RES2:
            *lux = (float)raw_value / 2.4f;  // 高分辨率模式2
            break;
            
        default:
            return GY30_ERROR;
    }
    
    return GY30_OK;
}

/**
 * @brief 重置GY-30传感器
 */
GY30_Status_t GY30_Reset(GY30_Device_t *device) {
    if (device == NULL) {
        return GY30_ERROR;
    }
    
    GY30_Status_t status = GY30_WriteCommand(device, BH1750_RESET);
    if (status == GY30_OK) {
        osDelay(10);  // 等待重置完成
        device->last_read_time = GY30_GetTickMs();
    }
    
    return status;
}

/**
 * @brief 进入睡眠模式
 */
GY30_Status_t GY30_Sleep(GY30_Device_t *device) {
    if (device == NULL) {
        return GY30_ERROR;
    }
    return GY30_WriteCommand(device, BH1750_POWER_DOWN);;
}

/**
 * @brief 从睡眠模式唤醒
 */
GY30_Status_t GY30_Wakeup(GY30_Device_t *device) {
    if (device == NULL) {
        return GY30_ERROR;
    }
    
    GY30_Status_t status = GY30_WriteCommand(device, BH1750_POWER_ON);
    if (status == GY30_OK) {
        osDelay(10);  // 等待上电稳定
    }
    return status;
}

/**
 * @brief 检查传感器是否在线
 */
GY30_Status_t GY30_IsOnline(GY30_Device_t *device) {
    if (device == NULL) {
        return GY30_ERROR;
    }
    
    HAL_StatusTypeDef hal_status = HAL_ERROR;
    
#if GY30_USE_I2C_BUS_MANAGER
    if (!I2C_Bus_Lock(GY30_I2C_LOCK_TIMEOUT_MS)) {
        LOG_ERROR("检查GY30设备在线时获取I2C总线锁失败");
        return GY30_TIMEOUT;
    }
#endif
        hal_status = HAL_I2C_IsDeviceReady(
            GY30_I2C_HANDLE, 
            device->addr << 1, 
            1,
            GY30_DEFAULT_TIMEOUT
        );
        
#if GY30_USE_I2C_BUS_MANAGER
    I2C_Bus_Unlock();
#endif
    if (hal_status == HAL_OK) {
        return GY30_OK;
    } else {
        LOG_DEBUG("GY30设备在地址 0x%02X 未响应 (HAL Status: %d)", device->addr, hal_status);
        return GY30_ERROR;
    }
}

/**
 * @brief 设置GY-30工作模式
 */
GY30_Status_t GY30_SetMode(GY30_Device_t *device, GY30_Mode_t mode) {
    if (device == NULL || !device->is_initialized) {
        return GY30_ERROR;
    }
    
    GY30_Status_t status = GY30_WriteCommand(device, (uint8_t)mode);
    if (status == GY30_OK) {
        device->mode = mode;
        device->last_read_time = GY30_GetTickMs();
    }
    
    return status;
}

/**
 * @brief 获取测量模式对应的等待时间
 */
uint32_t GY30_GetMeasurementTime(GY30_Mode_t mode) {
    switch (mode) {
        case GY30_MODE_LOW_RES:
        case GY30_MODE_ONE_LOW_RES:
            return 24;      // 低分辨率模式: 16ms (典型) + 裕量
            
        case GY30_MODE_HIGH_RES:
        case GY30_MODE_HIGH_RES2:
        case GY30_MODE_ONE_HIGH_RES:
        case GY30_MODE_ONE_HIGH_RES2:
            return 180;     // 高分辨率模式: 120ms (典型) + 裕量
            
        default:
            return 180;     // 默认使用较长的等待时间
    }
}

/* --------------------------- 私有函数实现 --------------------------- */

/**
 * @brief 向GY-30发送命令
 */
static GY30_Status_t GY30_WriteCommand(GY30_Device_t *device, uint8_t command) {
    LOG_DEBUG("向GY30地址 0x%02X 发送命令: 0x%02X", device->addr, command);
    HAL_StatusTypeDef hal_status;

#if GY30_USE_I2C_BUS_MANAGER
    if (!I2C_Bus_Lock(GY30_I2C_LOCK_TIMEOUT_MS)) {
        LOG_ERROR("GY30发送命令时获取I2C总线锁失败");
        return GY30_TIMEOUT;
    }
#endif
        hal_status = HAL_I2C_Master_Transmit(
            GY30_I2C_HANDLE,
            device->addr << 1,
            &command,
            1,
            GY30_DEFAULT_TIMEOUT
        );
#if GY30_USE_I2C_BUS_MANAGER
        I2C_Bus_Unlock();
#endif
    
    switch (hal_status) {
        case HAL_OK:        return GY30_OK;
        case HAL_TIMEOUT: LOG_ERROR("GY30 I2C发送命令超时"); return GY30_TIMEOUT;
        default:          LOG_ERROR("GY30 I2C发送命令失败, HAL Status: %d", hal_status); return GY30_ERROR;
    }
}

/**
 * @brief 从GY-30读取数据
 */
static GY30_Status_t GY30_ReadData(GY30_Device_t *device, uint8_t *data, uint16_t size) {
    LOG_DEBUG("从GY30地址 0x%02X 读取 %d 字节数据", device->addr, size);
    HAL_StatusTypeDef hal_status;
    
#if GY30_USE_I2C_BUS_MANAGER
    if (!I2C_Bus_Lock(GY30_I2C_LOCK_TIMEOUT_MS)) {
        LOG_ERROR("GY30读取数据时获取I2C总线锁失败");
        return GY30_TIMEOUT;
    }
#endif
    
    hal_status = HAL_I2C_Master_Receive(
        GY30_I2C_HANDLE, 
        device->addr << 1, 
        data, 
        size, 
        GY30_DEFAULT_TIMEOUT
    );

#if GY30_USE_I2C_BUS_MANAGER
    I2C_Bus_Unlock();
#endif
    
    switch (hal_status) {
        case HAL_OK:      return GY30_OK;
        case HAL_TIMEOUT: LOG_ERROR("GY30 I2C读取数据超时"); return GY30_TIMEOUT;
        default:          LOG_ERROR("GY30 I2C读取数据失败, HAL Status: %d", hal_status); return GY30_ERROR;
    }
}

/**
 * @brief 获取系统时间戳 (毫秒)
 */
static uint32_t GY30_GetTickMs(void) {
    return HAL_GetTick();
}

/**
 * @brief 执行带重试的操作
 * @param device GY30设备指针
 * @param func 操作函数指针
 * @param action_name 操作名称，用于日志输出
 * @return GY30_Status_t 执行结果
 */
static GY30_Status_t GY30_ExecuteWithRetry(GY30_Device_t* device, 
                                        GY30_Status_t (*func)(GY30_Device_t*), 
                                        const char* action_name) {
    const int max_retries = 3;
    GY30_Status_t status;
    for (int i = 0; i < max_retries; i++) {
        status = func(device);
        if (status == GY30_OK) {
            LOG_DEBUG("操作 '%s' 成功", action_name);
            return GY30_OK;
        }
        LOG_WARN("操作 '%s' 失败 (尝试 %d/%d)，等待重试...", action_name, i + 1, max_retries);
        osDelay(1000);
    }

    return status;
}

/**
 * @brief 将当前工作模式应用到设备
 */
static GY30_Status_t GY30_ApplyMode(GY30_Device_t *device) {
    if (device == NULL) {
        return GY30_ERROR;
    }
    device->is_initialized = true;  // 临时标记为已初始化
    return GY30_SetMode(device, device->mode);
}
