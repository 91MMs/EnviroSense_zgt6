/**
 ******************************************************************************
 * @file    motor.h
 * @brief   直流电机驱动头文件
 * @details 提供基于 PWM 的直流电机速度控制，支持自动/手动两种模式
 *          自动模式：通过电位器 ADC 值实时调速
 *          手动模式：通过软件接口设置固定速度
 * @author  EnviroSense Team
 * @date    2025
 ******************************************************************************
 */

#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 数据结构定义 ==================== */

/**
 * @brief 电机控制模式枚举
 */
typedef enum {
    MOTOR_MODE_AUTO,    /**< 自动模式：速度由电位器 ADC 值决定 */
    MOTOR_MODE_MANUAL   /**< 手动模式：速度由软件接口设置 */
} Motor_Control_Mode_t;

/**
 * @brief 电机驱动状态枚举
 */
typedef enum {
    MOTOR_OK = 0,       /**< 操作成功 */
    MOTOR_ERROR         /**< 操作失败 */
} Motor_Status_t;

/* ==================== 基础控制函数 ==================== */

/**
 * @brief  初始化电机控制系统
 * @retval MOTOR_OK: 初始化成功
 * @retval MOTOR_ERROR: ADC DMA 或 PWM 启动失败
 * @note   会启动 TIM1_CH1 的 PWM 输出（控制电机速度）
 * @note   会启动 ADC1 的 DMA 连续采集（通道包括电位器和传感器）
 * @note   初始状态为自动模式，电机速度为 0
 * @note   工作原理：
 *         - PWM 占空比（0-999）→ 电机速度（0-100%）
 *         - ADC DMA 缓冲区：[0]=传感器，[1]=电位器
 */
Motor_Status_t Motor_Init(void);

/**
 * @brief  设置电机控制模式
 * @param  mode: 控制模式
 *               - MOTOR_MODE_AUTO: 速度跟随电位器
 *               - MOTOR_MODE_MANUAL: 速度由 Motor_SetSpeed() 设置
 * @note   切换到自动模式时会立即根据电位器值更新速度
 * @note   手动模式下调用 Motor_SetSpeed() 才能改变速度
 */
void Motor_SetControlMode(Motor_Control_Mode_t mode);

/**
 * @brief  获取当前控制模式
 * @retval Motor_Control_Mode_t: 当前模式
 */
Motor_Control_Mode_t Motor_GetControlMode(void);

/**
 * @brief  手动设置电机速度（仅手动模式有效）
 * @param  duty: PWM 占空比（0-999）
 *               0 = 停止，999 = 最大速度
 * @note   ⚠️ 仅在 MOTOR_MODE_MANUAL 模式下生效
 * @note   在自动模式下调用此函数无效（会被电位器值覆盖）
 * @note   超过 999 会自动截断为 999
 */
void Motor_SetSpeed(uint16_t duty);

/**
 * @brief  获取电位器 ADC 原始值
 * @retval uint16_t: 电位器 ADC 值（0-4095）
 * @note   对应 12 位 ADC 的采样范围
 * @note   0 = 0V（最小），4095 = Vref（最大，通常 3.3V）
 * @note   数据来自 DMA 缓冲区，实时更新
 */
uint16_t Motor_GetPotValue(void);

/**
 * @brief  获取电机当前速度
 * @retval uint16_t: 当前 PWM 占空比（0-999）
 * @note   返回值为实际硬件输出的占空比
 * @note   可用于 UI 显示或调试
 */
uint16_t Motor_GetCurrentSpeed(void);

/**
 * @brief  周期性更新电机速度（自动模式）
 * @note   应在主循环或定时任务中调用（建议周期 50-100ms）
 * @note   仅在自动模式下生效，根据电位器值实时调速
 * @note   手动模式下调用无效果
 * @note   工作流程：
 *         1. 读取电位器 ADC 值
 *         2. 映射到 PWM 占空比（0-999）
 *         3. 更新硬件 PWM 输出
 */
void Motor_Update(void);

#ifdef __cplusplus
}
#endif

#endif // __MOTOR_H
