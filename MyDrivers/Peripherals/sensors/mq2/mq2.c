/**
 * @file mq2.c
 * @brief MQ-2 烟雾传感器驱动源文件
 * @author MmsY
 * @date 2025
*/

#include "mq2.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* --------------------------- 日志配置 --------------------------- */
#define LOG_MODULE "MQ2"
#include "log.h"

/* --------------------------- 共有宏定义 --------------------------- */
/* 声明adc_dma_buffer[]定义 */
/* mq2与motor公用了adc，这里没有添加adc_manager文件，只是简单的extern共用数组 */
/* mq2在dma中的索引是 0， motor索引为1 */
// uint16_t adc_dma_buffer[2];
extern uint16_t adc_dma_buffer[];

/* --------------------------- 私有函数声明 --------------------------- */
static float MQ2_ResistanceCalculation(uint16_t raw_adc);
static float MQ2_ReadSensor(void);
static uint32_t MQ2_GetTickMs(void);
static MQ2_Status_t MQ2_ExecuteWithRetry(MQ2_Device_t *device, 
                                        MQ2_Status_t (*func)(MQ2_Device_t *), 
                                        const char *action_name);

/* --------------------------- 公共函数实现 --------------------------- */

/**
 * @brief 初始化MQ-2传感器
 */
MQ2_Status_t MQ2_Init(MQ2_Device_t *device) {
    if (device == NULL) {
        return MQ2_ERROR;
    }

    if (device->is_initialized) {
        LOG_DEBUG("MQ-2设备已经初始化，无需重复初始化");
        return MQ2_OK;
    }

    LOG_INFO("开始初始化MQ-2设备");
    
    // 初始化设备结构体
    memset(device, 0, sizeof(MQ2_Device_t));
    device->r0 = 0.0f;
    device->is_initialized = false;
    device->is_calibrated = false;
    device->last_read_time = 0;

    // 执行校准
    if (MQ2_ExecuteWithRetry(device, MQ2_Calibrate, "校准传感器") != MQ2_OK) {
        LOG_ERROR("MQ-2传感器校准失败");
        return MQ2_ERROR;
    }

    device->is_initialized = true;
    LOG_INFO("MQ-2设备初始化成功, R0 = %.2f kΩ", device->r0);
    return MQ2_OK;
}

/**
 * @brief 校准MQ-2传感器
 */
MQ2_Status_t MQ2_Calibrate(MQ2_Device_t *device) {
    if (device == NULL) {
        return MQ2_ERROR;
    }

    LOG_INFO("开始校准MQ-2传感器 (请确保在清洁空气中)...");
    
    float rs_sum = 0.0f;
    
    for (int i = 0; i < MQ2_CALIBRATION_SAMPLE_TIMES; i++) {
        float rs = MQ2_ReadSensor();

        rs_sum += rs;
        osDelay(MQ2_CALIBRATION_SAMPLE_INTERVAL);
        
        if ((i + 1) % 10 == 0) {
            LOG_DEBUG("校准进度: %d/%d", i + 1, MQ2_CALIBRATION_SAMPLE_TIMES);
        }
    }
    
    float rs_avg = rs_sum / MQ2_CALIBRATION_SAMPLE_TIMES;
    device->r0 = rs_avg / MQ2_CLEAN_AIR_FACTOR;
    
    if (device->r0 <= 0.0f || device->r0 > 100.0f) {
        LOG_ERROR("MQ-2校准失败，R0值异常: %.2f", device->r0);
        return MQ2_ERROR;
    }
    
    device->is_calibrated = true;
    LOG_INFO("MQ-2传感器校准完成, R0 = %.2f kΩ", device->r0);
    
    return MQ2_OK;
}

/**
 * @brief 读取烟雾浓度 (PPM)
 */
MQ2_Status_t MQ2_ReadPPM(MQ2_Device_t *device, int *ppm) {
    if (device == NULL || ppm == NULL || !device->is_initialized) {
        return MQ2_ERROR;
    }

    if (!device->is_calibrated) {
        LOG_WARN("MQ-2传感器未校准，返回错误");
        return MQ2_NOT_CALIBRATED;
    }

    // 多次采样取平均值
    float rs_sum = 0.0f;
    for (int i = 0; i < MQ2_READ_SAMPLE_TIMES; i++) {
        rs_sum += MQ2_ReadSensor();
        if (i < MQ2_READ_SAMPLE_TIMES - 1) {
            osDelay(MQ2_READ_SAMPLE_INTERVAL);
        }
    }
    float rs = rs_sum / MQ2_READ_SAMPLE_TIMES;
    
    // 计算 RS/R0 比值
    float ratio = rs / device->r0;
    
    // MQ-2的PPM计算公式 (根据数据手册曲线拟合)
    // 这里使用简化的指数模型: PPM = A * (RS/R0)^B
    // 对于烟雾检测，典型参数为: A ≈ 613.9, B ≈ -2.074
    // 注意：这些参数可能需要根据实际传感器进行调整
    float ppm_value = 613.9f * powf(ratio, -2.074f);
    
    // 限制范围 (MQ-2典型范围: 200-10000 PPM)
    if (ppm_value < 0) ppm_value = 0;
    if (ppm_value > 10000) ppm_value = 10000;
    
    *ppm = (int)ppm_value;
    printf("MQ2 Read PPM: RS=%.2f kΩ, R0=%.2f kΩ, Ratio=%.2f, PPM=%d\n", rs, device->r0, ratio, *ppm);
    device->last_read_time = MQ2_GetTickMs();
    
    return MQ2_OK;
}

/**
 * @brief 读取传感器电阻值 (RS)
 */
MQ2_Status_t MQ2_ReadResistance(MQ2_Device_t *device, float *rs) {
    if (device == NULL || rs == NULL || !device->is_initialized) {
        return MQ2_ERROR;
    }

    *rs = MQ2_ReadSensor();
    device->last_read_time = MQ2_GetTickMs();
    
    return MQ2_OK;
}

/**
 * @brief 读取原始ADC值 (支持DMA和轮询两种模式)
 */
MQ2_Status_t MQ2_ReadRawValue(uint16_t *raw_value) {
    if (raw_value == NULL) {
        return MQ2_ERROR;
    }

#if MQ2_USE_DMA_MODE == 1
    /********************* DMA模式 *********************/
    // 直接从后台DMA缓冲区读取预先采集好的数据。
    // 根据CubeMX的Rank配置，MQ2(Channel 3)是第1个通道，对应索引MQ2_ADC_DMA_BUFFER_INDEX=0。
    *raw_value = adc_dma_buffer[MQ2_ADC_DMA_BUFFER_INDEX];
    return MQ2_OK;

#else
    /******************* 轮询模式 (非DMA) *******************/
    // 这种模式会独占式地使用ADC，在多任务或多ADC通道环境下需谨慎使用。
    HAL_StatusTypeDef status;

    // 启动ADC转换
    status = HAL_ADC_Start(MQ2_ADC_HANDLE);
    if (status != HAL_OK) {
        LOG_ERROR("轮询模式：启动ADC转换失败 (HAL Status: %d)", status);
        return MQ2_ERROR;
    }
    
    // 等待转换完成
    status = HAL_ADC_PollForConversion(MQ2_ADC_HANDLE, MQ2_DEFAULT_TIMEOUT);
    if (status != HAL_OK) {
        LOG_ERROR("轮询模式：ADC转换超时 (HAL Status: %d)", status);
        HAL_ADC_Stop(MQ2_ADC_HANDLE); // 确保在出错时停止ADC
        return MQ2_TIMEOUT;
    }
    
    // 读取ADC值
    *raw_value = HAL_ADC_GetValue(MQ2_ADC_HANDLE);
    
    // 停止ADC
    HAL_ADC_Stop(MQ2_ADC_HANDLE);
    
    return MQ2_OK;
#endif
}

/**
 * @brief 检测是否有烟雾
 */
bool MQ2_IsSmoke(MQ2_Device_t *device, int threshold) {
    int ppm;
    
    if (MQ2_ReadPPM(device, &ppm) == MQ2_OK) {
        return (ppm > threshold);
    }
    
    return false;
}

/* --------------------------- 私有函数实现 --------------------------- */

/**
 * @brief 根据ADC值计算传感器电阻
 */
static float MQ2_ResistanceCalculation(uint16_t raw_adc) {
    // 防止除零错误
    if (raw_adc == 0) {
        return 0.0f;
    }
    
    // 计算传感器两端的电压
    float sensor_volt = ((float)raw_adc / MQ2_ADC_RESOLUTION) * MQ2_VREF;
    
    // 使用分压公式计算传感器电阻 RS
    // VCC -> RS -> RL -> GND
    // sensor_volt = VCC * RL / (RS + RL)
    // RS = RL * (VCC / sensor_volt - 1)
    
    // 防止除零
    if (sensor_volt <= 0.001f) {
        return 0.0f;
    }
    
    float rs = MQ2_RL_VALUE * ((MQ2_VREF / sensor_volt) - 1.0f);
    
    // 限制合理范围
    if (rs < 0.0f) rs = 0.0f;
    if (rs > 1000.0f) rs = 1000.0f;  // 限制最大值
    
    return rs;
}

/**
 * @brief 读取传感器电阻值
 */
static float MQ2_ReadSensor(void) {
    uint16_t raw_value;
    
    if (MQ2_ReadRawValue(&raw_value) == MQ2_OK) {
        return MQ2_ResistanceCalculation(raw_value);
    }
    
    return 0.0f;
}

/**
 * @brief 获取系统时间戳 (毫秒)
 */
static uint32_t MQ2_GetTickMs(void) {
    return HAL_GetTick();
}

/**
 * @brief 执行带重试的操作
 */
static MQ2_Status_t MQ2_ExecuteWithRetry(MQ2_Device_t* device, 
                                        MQ2_Status_t (*func)(MQ2_Device_t*), 
                                        const char* action_name) {
    const int max_retries = 3;
    MQ2_Status_t status;
    
    for (int i = 0; i < max_retries; i++) {
        status = func(device);
        if (status == MQ2_OK) {
            LOG_DEBUG("操作 '%s' 成功", action_name);
            return MQ2_OK;
        }
        LOG_WARN("操作 '%s' 失败 (尝试 %d/%d)，等待重试...", action_name, i + 1, max_retries);
        osDelay(1000);
    }

    return status;
}
