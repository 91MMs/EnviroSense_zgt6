/**
 * @file sht30.c
 * @brief SHT30 温湿度传感器驱动源文件
 * @author MmsY (Adapted for SHT30)
 * @date 2025
*/

#include "sht30.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
#include "i2c_bus_manager.h"

/* --------------------------- 调试宏定义 --------------------------- */
#define LOG_MODULE "SHT30"
#include "log.h"

#define SHT30_DEFAULT_TIMEOUT    200
#define SHT30_I2C_LOCK_TIMEOUT_MS 100

/* --------------------------- 私有函数声明 --------------------------- */
static SHT30_Status_t SHT30_WriteCommand(SHT30_Device_t *device, const uint8_t *command, uint16_t size);
static SHT30_Status_t SHT30_ReadData(SHT30_Device_t *device, uint8_t *data, uint16_t size);
static uint8_t SHT30_CheckCrc(uint8_t *data, uint8_t len);
static SHT30_Status_t SHT30_ExecuteWithRetry(SHT30_Device_t *device, 
                                            SHT30_Status_t (*func)(SHT30_Device_t *), 
                                            const char *action_name);
/* --------------------------- 公共函数实现 --------------------------- */

/**
 * @brief 初始化SHT30传感器
 */
SHT30_Status_t SHT30_Init(SHT30_Device_t *device, uint8_t i2c_addr) {
    if (device == NULL) return SHT30_ERROR;

    if (device->is_initialized) {
        LOG_DEBUG("SHT30设备已经初始化，无需重复初始化");
        return SHT30_OK;
    }

    LOG_INFO("开始初始化SHT30设备, I2C地址: 0x%02X", i2c_addr);

    // 初始化设备结构体
    memset(device, 0, sizeof(SHT30_Device_t));
    device->addr = i2c_addr;
    device->is_initialized = false;
    
    // 初始化设备
    if (SHT30_ExecuteWithRetry(device, SHT30_IsOnline, "查询设备在线状态") != SHT30_OK) {
        return SHT30_ERROR;
    }
    if (SHT30_ExecuteWithRetry(device, SHT30_Reset, "复位设备") != SHT30_OK) {
        return SHT30_ERROR;
    }

    device->is_initialized = true;          // 标记为已初始化
    return SHT30_OK;
}

/**
 * @brief 获取SHT30传感器温湿度数据
 */
SHT30_Status_t SHT30_ReadTempHumi(SHT30_Device_t *device, float *temp, float *humi) {
    if (device == NULL || temp == NULL || humi == NULL || !device->is_initialized) {
        return SHT30_ERROR;
    }

    // 发送单次测量命令
    uint8_t cmd[2] = SHT30_CMD_MEAS_SINGLE_H;
    SHT30_Status_t status = SHT30_WriteCommand(device, cmd, 2);
    if (status != SHT30_OK) return status;

    // 等待测量完成 (高精度模式约15ms)
    osDelay(20);

    // 读取6字节数据 (Temp_MSB, Temp_LSB, Temp_CRC, Humi_MSB, Humi_LSB, Humi_CRC)
    uint8_t data[6];
    status = SHT30_ReadData(device, data, 6);
    if (status != SHT30_OK) return status;

    // 校验温度CRC
    if (SHT30_CheckCrc(data, 2) != data[2]) {
        LOG_ERROR("温度数据CRC校验失败!");
        return SHT30_CRC_ERROR;
    }

    // 校验湿度CRC
    if (SHT30_CheckCrc(data + 3, 2) != data[5]) {
        LOG_ERROR("湿度数据CRC校验失败!");
        return SHT30_CRC_ERROR;
    }

    // 计算温度和湿度
    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_humi = (data[3] << 8) | data[4];

    *temp = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humi = 100.0f * ((float)raw_humi / 65535.0f);

    return SHT30_OK;
}

/**
 * @brief 复位SHT30传感器
 */ 
SHT30_Status_t SHT30_Reset(SHT30_Device_t *device) {
    if (device == NULL) {
        return SHT30_ERROR;
    }

    uint8_t cmd[2] = SHT30_CMD_RESET;
    SHT30_Status_t status = SHT30_WriteCommand(device, cmd, 2);
    if (status == SHT30_OK) {
        osDelay(10); // 等待复位完成
    }
    return status;
}

/**
 * @brief 检查SHT30传感器是否在线
 */ 
SHT30_Status_t SHT30_IsOnline(SHT30_Device_t *device) {
    if (device == NULL) {
        return SHT30_ERROR;
    }

    HAL_StatusTypeDef hal_status = HAL_ERROR;
    
#if SHT30_USE_I2C_BUS_MANAGER
    if (!I2C_Bus_Lock(SHT30_I2C_LOCK_TIMEOUT_MS)) {
        LOG_ERROR("SHT30检查在线时获取I2C总线锁失败");
        return SHT30_TIMEOUT;
    }
#endif
        hal_status = HAL_I2C_IsDeviceReady(
            SHT30_I2C_HANDLE, 
            device->addr << 1, 
            1, 
            SHT30_DEFAULT_TIMEOUT
        );
#if (SHT30_USE_I2C_BUS_MANAGER == 1)
        I2C_Bus_Unlock();
#endif
    if (hal_status == HAL_OK) {
        return SHT30_OK;
    } else {
        LOG_DEBUG("SHT30设备在地址 0x%02X 未响应 (HAL Status: %d)", device->addr, hal_status);
        return SHT30_ERROR;
    }
}

/* --------------------------- 私有函数实现 --------------------------- */

/**
 * @brief 向SHT30设备发送命令
 */ 
static SHT30_Status_t SHT30_WriteCommand(SHT30_Device_t *device, const uint8_t *command, uint16_t size) {
    if (device == NULL || command == NULL || size == 0) {
        return SHT30_ERROR;
    }
    LOG_DEBUG("向SHT30地址 0x%02X 发送命令", device->addr);
    HAL_StatusTypeDef hal_status;

#if SHT30_USE_I2C_BUS_MANAGER
    if (!I2C_Bus_Lock(SHT30_I2C_LOCK_TIMEOUT_MS)) {
        LOG_ERROR("SHT30发送命令时获取I2C总线锁失败");
        return SHT30_TIMEOUT;
    }
#endif
    hal_status = HAL_I2C_Master_Transmit(
        SHT30_I2C_HANDLE, 
        device->addr << 1, 
        (uint8_t*)command, 
        size, 
        SHT30_DEFAULT_TIMEOUT
    );
#if (SHT30_USE_I2C_BUS_MANAGER == 1)
    I2C_Bus_Unlock();
#endif
    switch (hal_status) {
        case HAL_OK:      return SHT30_OK;
        case HAL_TIMEOUT: LOG_ERROR("SHT30 I2C发送命令超时"); return SHT30_TIMEOUT;
        default:          LOG_ERROR("SHT30 I2C发送命令失败, HAL Status: %d", hal_status); return SHT30_ERROR;
    }
}

/**
 * @brief 从SHT30设备读取数据
 */ 
static SHT30_Status_t SHT30_ReadData(SHT30_Device_t *device, uint8_t *data, uint16_t size) {
    if (device == NULL || data == NULL) {
        return SHT30_ERROR;
    }
    LOG_DEBUG("从SHT30地址 0x%02X 读取 %d 字节数据", device->addr, size);
    HAL_StatusTypeDef hal_status;

#if SHT30_USE_I2C_BUS_MANAGER
    if (!I2C_Bus_Lock(SHT30_I2C_LOCK_TIMEOUT_MS)) {
        LOG_ERROR("SHT30读取数据时获取I2C总线锁失败");
        return SHT30_TIMEOUT;
    }
#endif
    hal_status = HAL_I2C_Master_Receive(
        SHT30_I2C_HANDLE, 
        device->addr << 1, 
        data, 
        size, 
        SHT30_DEFAULT_TIMEOUT
    );
#if (SHT30_USE_I2C_BUS_MANAGER == 1)
    I2C_Bus_Unlock();
#endif
    switch (hal_status) {
        case HAL_OK:      return SHT30_OK;
        case HAL_TIMEOUT: LOG_ERROR("SHT30 I2C读取数据超时"); return SHT30_TIMEOUT;
        default:          LOG_ERROR("SHT30 I2C读取数据失败, HAL Status: %d", hal_status); return SHT30_ERROR;
    }
}

/**
 * @brief CRC校验函数
 */
static uint8_t SHT30_CheckCrc(uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 8; j > 0; --j) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

/**
 * @brief 执行带重试的操作  
 */
static SHT30_Status_t SHT30_ExecuteWithRetry(SHT30_Device_t *device,
                                            SHT30_Status_t (*func)(SHT30_Device_t *),
                                            const char *action_name) {
    const int max_retries = 3;
    SHT30_Status_t status;
    for (int i = 0; i < max_retries; i++) {
        status = func(device);
        if (status == SHT30_OK) {
            LOG_DEBUG("操作 '%s' 成功", action_name);
            return SHT30_OK;
        }
        LOG_WARN("操作 '%s' 失败 (尝试 %d/%d)，等待重试...", action_name, i + 1, max_retries);
        osDelay(100);
    }
    LOG_ERROR("经过 %d 次尝试, 操作 '%s' 仍失败", max_retries, action_name);
    return status;                                            
}
