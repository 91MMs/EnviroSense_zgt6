/**
 * @file mq2.h
 * @brief MQ-2 烟雾传感器驱动头文件
 * @author MmsY
 * @date 2025
*/

#ifndef __MQ2_H
#define __MQ2_H

#include "main.h"
#include "adc.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* --------------------------- 硬件配置 --------------------------- */
// 根据您的CubeMX配置修改ADC句柄名称
extern ADC_HandleTypeDef hadc1;
#define MQ2_ADC_HANDLE          (&hadc1)
#define MQ2_ADC_CHANNEL         ADC_CHANNEL_3  // 根据实际连接的ADC通道修改

// [新增] 定义一个宏来控制ADC的读取模式
// 设置为 1: 使用DMA模式，直接从全局缓冲区读取
// 设置为 0: 使用阻塞轮询模式，独立读取ADC
#define MQ2_USE_DMA_MODE    1
// 在DMA模式下，MQ-2数据在adc_dma_buffer[]中的索引
#if MQ2_USE_DMA_MODE 
    #define MQ2_ADC_DMA_BUFFER_INDEX  0  
#endif

/* --------------------------- MQ-2传感器参数 --------------------------- */
#define MQ2_ADC_RESOLUTION      4095    // 12位ADC的最大值
#define MQ2_VREF                3.3f    // 参考电压 (V)
#define MQ2_RL_VALUE            1.0f    // 负载电阻值 (kΩ)，根据实际电路调整
#define MQ2_CLEAN_AIR_FACTOR    9.83f   // 清洁空气中的RS/R0比值

// 校准参数
#define MQ2_CALIBRATION_SAMPLE_TIMES    50      // 校准采样次数
#define MQ2_CALIBRATION_SAMPLE_INTERVAL 50      // 校准采样间隔 (ms)
#define MQ2_READ_SAMPLE_TIMES           5       // 读取时采样次数
#define MQ2_READ_SAMPLE_INTERVAL        50      // 读取采样间隔 (ms)

/* --------------------------- 数据类型定义 --------------------------- */
// MQ-2状态枚举
typedef enum {
    MQ2_OK              = 0x00,     // 操作成功
    MQ2_ERROR           = 0x01,     // 一般错误
    MQ2_NOT_CALIBRATED  = 0x02,     // 未校准
    MQ2_TIMEOUT         = 0x03      // 超时错误
} MQ2_Status_t;

// MQ-2设备结构体
typedef struct {
    float r0;                   // 传感器在清洁空气中的基准电阻值
    bool is_initialized;        // 初始化标志
    bool is_calibrated;         // 校准标志
    uint32_t last_read_time;    // 上次读取时间 (ms)
} MQ2_Device_t;

/* --------------------------- 公共函数声明 --------------------------- */

/**
 * @brief 初始化MQ-2传感器
 * @param device MQ-2设备结构体指针
 * @return MQ2_Status_t 初始化状态
 */
MQ2_Status_t MQ2_Init(MQ2_Device_t *device);

/**
 * @brief 校准MQ-2传感器 (在清洁空气中进行)
 * @param device MQ-2设备结构体指针
 * @return MQ2_Status_t 校准状态
 */
MQ2_Status_t MQ2_Calibrate(MQ2_Device_t *device);

/**
 * @brief 读取烟雾浓度 (PPM)
 * @param device MQ-2设备结构体指针
 * @param ppm 输出的烟雾浓度值 (单位: PPM)
 * @return MQ2_Status_t 操作状态
 */
MQ2_Status_t MQ2_ReadPPM(MQ2_Device_t *device, int *ppm);

/**
 * @brief 读取传感器电阻值 (RS)
 * @param device MQ-2设备结构体指针
 * @param rs 输出的传感器电阻值 (单位: kΩ)
 * @return MQ2_Status_t 操作状态
 */
MQ2_Status_t MQ2_ReadResistance(MQ2_Device_t *device, float *rs);

/**
 * @brief 读取原始ADC值
 * @param raw_value 输出的原始ADC值
 * @return MQ2_Status_t 操作状态
 */
MQ2_Status_t MQ2_ReadRawValue(uint16_t *raw_value);

/**
 * @brief 检测是否有烟雾 (简单阈值检测)
 * @param device MQ-2设备结构体指针
 * @param threshold PPM阈值
 * @return true: 检测到烟雾, false: 未检测到
 */
bool MQ2_IsSmoke(MQ2_Device_t *device, int threshold);

/* --------------------------- 便利宏定义 --------------------------- */
#define MQ2_DEFAULT_TIMEOUT    1000    // 默认超时时间 (ms)

#ifdef  __cplusplus
}
#endif

#endif /* __MQ2_H */
