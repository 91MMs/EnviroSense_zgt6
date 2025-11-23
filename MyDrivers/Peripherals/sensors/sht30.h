/**
 * @file sht30.h
 * @brief SHT30 温湿度传感器驱动头文件
 * @author MmsY (Adapted for SHT30)
 * @date 2025
*/

#ifndef __SHT30_H
#define __SHT30_H

#include "main.h"
#include "i2c.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* --------------------------- 硬件配置 --------------------------- */
extern I2C_HandleTypeDef hi2c1;
#define SHT30_I2C_HANDLE         (&hi2c1)
#define SHT30_USE_I2C_BUS_MANAGER 1

/* --------------------------- SHT30寄存器和命令 --------------------------- */
#define SHT30_DEFAULT_ADDR  0x44    // 默认I2C地址

// SHT30指令 (2字节)
#define SHT30_CMD_MEAS_SINGLE_H {0x2C, 0x06} // 单次测量，高精度
#define SHT30_CMD_RESET         {0x30, 0xA2} // 软复位

/* --------------------------- 数据类型定义 --------------------------- */
typedef enum {
    SHT30_OK         = 0x00,    // 操作成功
    SHT30_ERROR      = 0x01,    // 一般错误
    SHT30_TIMEOUT    = 0x02,    // 超时错误
    SHT30_CRC_ERROR  = 0x03     // CRC校验错误
} SHT30_Status_t;

typedef struct {
    uint8_t addr;               // I2C地址
    bool is_initialized;        // 初始化标志
} SHT30_Device_t;

/* --------------------------- 公共函数声明 --------------------------- */
/**
 * @brief 初始化SHT30传感器
 * @param device SHT30设备结构体指针
 * @param i2c_addr I2C地址 (通常是SSHT30_DEFAULT_ADDR)
 * @return SHT30_Status_t 初始化状态
 */
SHT30_Status_t SHT30_Init(SHT30_Device_t *device, uint8_t i2c_addr);

/**
 * @brief 读取温度和湿度 (阻塞式)
 * @param device SHT30设备结构体指针
 * @param temp 输出的温度值 (单位: °C)
 * @param humi 输出的湿度值 (单位: %RH)
 * @return SHT30_Status_t 操作状态
 */
SHT30_Status_t SHT30_ReadTempHumi(SHT30_Device_t *device, float *temp, float *humi);

/**
 * @brief 复位SHT30传感器
 * @param device SHT30设备结构体指针
 * @return SHT30_Status_t 操作状态
 */
SHT30_Status_t SHT30_Reset(SHT30_Device_t *device);

/**
 * @brief 检查传感器是否在线
 * @param device SHT30设备结构体指针
 * @return SHT30_Status_t 操作状态
 */
SHT30_Status_t SHT30_IsOnline(SHT30_Device_t *device);


#ifdef  __cplusplus
}
#endif

#endif /* __SHT30_H */
