/**
 ******************************************************************************
 * @file    motor.c
 * @brief   直流电机驱动实现
 * @details 基于 STM32 TIM1 PWM 输出控制直流电机速度
 *          通过 ADC DMA 采集电位器值，实现自动调速功能
 * @author  EnviroSense Team
 * @date    2025
 ******************************************************************************
 */

#include "motor.h"
#include "adc.h"
#include "tim.h"

#define LOG_MODULE "MOTOR"
#include "log.h"

/* ==================== 外部引用 ==================== */

/**
 * @brief ADC1 句柄（在 CubeMX 生成的 adc.c 中定义）
 */
extern ADC_HandleTypeDef hadc1;

/**
 * @brief TIM1 句柄（在 CubeMX 生成的 tim.c 中定义）
 */
extern TIM_HandleTypeDef htim1;

/* ==================== 静态变量 ==================== */

/**
 * @brief ADC DMA 采集缓冲区
 * @note  [0] = 传感器通道（如 MQ-2 烟雾传感器）
 * @note  [1] = 电位器通道（用于电机速度控制）
 * @note  DMA 循环模式，数据自动更新
 */
extern uint16_t adc_dma_buffer[];

/**
 * @brief 当前控制模式
 * @note  默认为自动模式（跟随电位器）
 */
static Motor_Control_Mode_t s_control_mode = MOTOR_MODE_AUTO;

/**
 * @brief 当前 PWM 占空比缓存（0-999）
 * @note  用于状态查询和避免重复设置
 */
static uint16_t s_current_pwm_duty = 0;

/* ==================== 私有函数 ==================== */

/**
 * @brief 将 ADC 值映射到 PWM 占空比
 * @param adc_val: ADC 原始值（0-4095）
 * @retval uint16_t: PWM 占空比（0-999）
 * 
 * @note 映射公式：duty = (adc / 4095) * 999
 * @note 结果自动截断到 999，确保不超过 TIM1 的 ARR 值
 */
static uint16_t map_adc_to_pwm(uint16_t adc_val)
{
    /* 线性映射：12 位 ADC → 10 位 PWM */
    uint16_t pwm_duty = (uint16_t)((adc_val / 4095.0f) * 999);
    
    /* 限制最大值（防止浮点误差导致溢出） */
    if (pwm_duty > 999) pwm_duty = 999;
    
    return pwm_duty;
}

/**
 * @brief 设置 PWM 硬件占空比
 * @param duty: 目标占空比（0-999）
 * 
 * @note 直接操作 TIM1_CH1 的 CCR 寄存器
 * @note 超过 999 会自动截断
 * @note 同时更新内部状态缓存
 */
static void motor_set_pwm_hw(uint16_t duty)
{
    /* 限制占空比范围 */
    if (duty > 999) duty = 999;
    
    /* 更新 TIM1 通道 1 的比较值（CCR 寄存器） */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty);
    
    /* 更新内部缓存 */
    s_current_pwm_duty = duty;
}

/* ==================== 公共函数实现 ==================== */

/**
 * @brief 初始化电机控制系统
 */
Motor_Status_t Motor_Init(void)
{
    /* 启动 ADC DMA 连续采集（2 通道：传感器 + 电位器） */
    // if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buffer, 2) != HAL_OK) {
    //     LOG_ERROR("电机 ADC DMA 启动失败");
    //     return MOTOR_ERROR;
    // }
    
    /* 启动 TIM1 通道 1 的 PWM 输出 */
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) {
        LOG_ERROR("电机 PWM 启动失败");
        return MOTOR_ERROR;
    }
    
    /* 设置默认控制模式为自动 */
    s_control_mode = MOTOR_MODE_AUTO;
    
    /* 初始速度为 0（安全考虑） */
    motor_set_pwm_hw(0);
    
    LOG_INFO("电机控制初始化完成（自动模式）");
    return MOTOR_OK;
}

/**
 * @brief 设置控制模式
 */
void Motor_SetControlMode(Motor_Control_Mode_t mode)
{
    s_control_mode = mode;
    
    /* 切换到自动模式时，立即根据电位器值更新速度 */
    if (s_control_mode == MOTOR_MODE_AUTO) {
        Motor_Update();
    }
}

/**
 * @brief 获取控制模式
 */
Motor_Control_Mode_t Motor_GetControlMode(void)
{
    return s_control_mode;
}

/**
 * @brief 手动设置速度（仅手动模式生效）
 */
void Motor_SetSpeed(uint16_t duty)
{
    /* 只有在手动模式下才允许设置速度 */
    if (s_control_mode == MOTOR_MODE_MANUAL) {
        motor_set_pwm_hw(duty);
    }
    /* 自动模式下忽略此调用（会被电位器值覆盖） */
}

/**
 * @brief 获取电位器 ADC 值
 */
uint16_t Motor_GetPotValue(void)
{
    /* 返回 DMA 缓冲区中的电位器通道数据 */
    return adc_dma_buffer[1];
}

/**
 * @brief 获取当前速度
 */
uint16_t Motor_GetCurrentSpeed(void)
{
    return s_current_pwm_duty;
}

/**
 * @brief 周期性更新电机速度（自动模式）
 * 
 * @note 建议在 FreeRTOS 任务中以 50-100ms 周期调用
 * @note 示例：
 *       @code
 *       void MotorControlTask(void const *argument)
 *       {
 *           for(;;)
 *           {
 *               Motor_Update();
 *               osDelay(100);
 *           }
 *       }
 *       @endcode
 */
void Motor_Update(void)
{
    /* 读取电位器 ADC 值（DMA 自动更新） */
    uint16_t pot_value = Motor_GetPotValue();
    
    /* 仅在自动模式下更新速度 */
    if (s_control_mode == MOTOR_MODE_AUTO) {
        /* 将 ADC 值映射到 PWM 占空比 */
        uint16_t new_duty = map_adc_to_pwm(pot_value);
        
        /* 更新硬件 PWM 输出 */
        motor_set_pwm_hw(new_duty);
    }
    /* 手动模式下不做任何操作 */
}
