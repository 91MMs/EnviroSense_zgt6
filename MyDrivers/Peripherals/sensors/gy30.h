/**
 * @file gy30.c
 * @brief GY-30 (BH1750) 光照传感器驱动源文件
 * @author MmsY
 * @date 2025
*/

#ifndef __GY30_H
#define __GY30_H

#include "main.h"
#include "i2c.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* --------------------------- 硬件配置 --------------------------- */
// 根据您的CubeMX配置修改I2C句柄名称，通常是hi2c1或hi2c2
extern I2C_HandleTypeDef hi2c1;  // 假设使用I2C1，请根据实际情况修改
#define GY30_I2C_HANDLE         (&hi2c1)

#ifndef GY30_USE_I2C_BUS_MANAGER
#define GY30_USE_I2C_BUS_MANAGER 1
#endif  

#if GY30_USE_I2C_BUS_MANAGER
#include "i2c_bus_manager.h"
#define GY30_I2C_LOCK_TIMEOUT_MS      100 
#endif


/* --------------------------- BH1750寄存器和命令 --------------------------- */
// BH1750 I2C地址 (ADDR引脚接GND: 0x23, 接VCC: 0x5C)
#define BH1750_ADDR_LOW         0x23    // ADDR pin = 0 (GND)
#define BH1750_ADDR_HIGH        0x5C    // ADDR pin = 1 (VCC)
#define BH1750_DEFAULT_ADDR     BH1750_ADDR_LOW

// BH1750指令
#define BH1750_POWER_DOWN       0x00    // 掉电模式
#define BH1750_POWER_ON         0x01    // 上电模式
#define BH1750_RESET            0x07    // 重置数字寄存器

// 测量模式
#define BH1750_CONT_H_MODE      0x10    // 连续H分辨率模式 (1lx分辨率)
#define BH1750_CONT_H_MODE2     0x11    // 连续H分辨率模式2 (0.5lx分辨率)
#define BH1750_CONT_L_MODE      0x13    // 连续L分辨率模式 (4lx分辨率)
#define BH1750_ONE_H_MODE       0x20    // 一次H分辨率模式 (1lx分辨率)
#define BH1750_ONE_H_MODE2      0x21    // 一次H分辨率模式2 (0.5lx分辨率)
#define BH1750_ONE_L_MODE       0x23    // 一次L分辨率模式 (4lx分辨率)

/* --------------------------- 数据类型定义 --------------------------- */
// GY-30工作模式枚举
typedef enum {
    GY30_MODE_LOW_RES       = BH1750_CONT_L_MODE,      // 连续低分辨率 (4lx, 16ms)
    GY30_MODE_HIGH_RES      = BH1750_CONT_H_MODE,      // 连续高分辨率 (1lx, 120ms)
    GY30_MODE_HIGH_RES2     = BH1750_CONT_H_MODE2,     // 连续高分辨率2 (0.5lx, 120ms)
    GY30_MODE_ONE_LOW_RES   = BH1750_ONE_L_MODE,       // 单次低分辨率
    GY30_MODE_ONE_HIGH_RES  = BH1750_ONE_H_MODE,       // 单次高分辨率
    GY30_MODE_ONE_HIGH_RES2 = BH1750_ONE_H_MODE2       // 单次高分辨率2
} GY30_Mode_t;

// GY-30状态枚举
typedef enum {
    GY30_OK         = 0x00,     // 操作成功
    GY30_ERROR      = 0x01,     // 一般错误
    GY30_TIMEOUT    = 0x02,     // 超时错误
    GY30_NOT_READY  = 0x03      // 数据未准备好
} GY30_Status_t;

// GY-30设备结构体
typedef struct {
    uint8_t addr;               // I2C地址
    GY30_Mode_t mode;           // 当前工作模式
    bool is_initialized;        // 初始化标志
    uint32_t last_read_time;    // 上次读取时间 (ms)
} GY30_Device_t;

/* --------------------------- 公共函数声明 --------------------------- */

/**
 * @brief 初始化GY-30传感器
 * @param device GY-30设备结构体指针
 * @param i2c_addr I2C地址 (通常是BH1750_DEFAULT_ADDR)
 * @return GY30_Status_t 初始化状态
 */
GY30_Status_t GY30_Init(GY30_Device_t *device, uint8_t i2c_addr);

/**
 * @brief 设置GY-30工作模式
 * @param device GY-30设备结构体指针
 * @param mode 工作模式
 * @return GY30_Status_t 操作状态
 */
GY30_Status_t GY30_SetMode(GY30_Device_t *device, GY30_Mode_t mode);

/**
 * @brief 读取光照强度值
 * @param device GY-30设备结构体指针
 * @param lux 输出的光照强度值 (单位: lux)
 * @return GY30_Status_t 操作状态
 */
GY30_Status_t GY30_ReadLux(GY30_Device_t *device, float *lux);

/**
 * @brief 重置GY-30传感器
 * @param device GY-30设备结构体指针
 * @return GY30_Status_t 操作状态
 */
GY30_Status_t GY30_Reset(GY30_Device_t *device);

/**
 * @brief 进入睡眠模式 (低功耗)
 * @param device GY-30设备结构体指针
 * @return GY30_Status_t 操作状态
 */
GY30_Status_t GY30_Sleep(GY30_Device_t *device);

/**
 * @brief 从睡眠模式唤醒
 * @param device GY-30设备结构体指针
 * @return GY30_Status_t 操作状态
 */
GY30_Status_t GY30_Wakeup(GY30_Device_t *device);

/**
 * @brief 检查传感器是否在线
 * @param device GY-30设备结构体指针
 * @return true: 在线, false: 离线
 */
GY30_Status_t GY30_IsOnline(GY30_Device_t *device);

/**
 * @brief 获取测量模式对应的等待时间 (ms)
 * @param mode 测量模式
 * @return uint32_t 等待时间 (毫秒)
 */
uint32_t GY30_GetMeasurementTime(GY30_Mode_t mode);

/* --------------------------- 便利宏定义 --------------------------- */
#define GY30_DEFAULT_TIMEOUT    200    // 默认超时时间 (ms)

#endif /* __GY30_H */
