/**
 ******************************************************************************
 * @file    devices_manager.h
 * @brief   外设设备统一管理器头文件
 * @details 提供蜂鸣器、RGB LED、电机的统一初始化和控制接口
 *          封装底层驱动，为上层应用（如 Dashboard）提供简洁的调用接口
 * @author  EnviroSense Team
 * @date    2025
 ******************************************************************************
 */

#ifndef __DEVICES_MANAGER_H
#define __DEVICES_MANAGER_H

#include "main.h"
#include <stdbool.h>
#include "buzzer.h"
#include "rgbled.h"
#include "motor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 数据结构定义 ==================== */

/**
 * @brief 设备初始化状态结构体
 * @note  用于查询各外设的就绪状态
 */
typedef struct {
    bool buzzer_ready;   /**< 蜂鸣器是否就绪 */
    bool rgb_led_ready;  /**< RGB LED 是否就绪 */
    bool motor_ready;    /**< 电机控制是否就绪 */
} Drivers_Status_t;

/* ==================== RGB LED 结构体 ====================*/
/**
 * @brief LED 手动模式颜色槽位枚举
 */
typedef enum {
    LED_STATE_OFF = 0,      /**< 关闭 */
    LED_STATE_SLOT_1,       /**< 颜色槽位 1 */
    LED_STATE_SLOT_2,       /**< 颜色槽位 2 */
    LED_STATE_SLOT_3        /**< 颜色槽位 3 */
} led_manual_state_t;

/**
 * @brief LED 控制模式枚举
 */
typedef enum {
    LED_MODE_MANUAL = 0,    /**< 手动模式：使用预设颜色槽位 */
    LED_MODE_AUTO           /**< 自动模式：根据光照传感器自动调节 */
} led_control_mode_t;

/* ==================== 系统管理接口 ==================== */

/**
 * @brief  统一初始化所有外设驱动
 * @retval true: 所有设备初始化成功
 * @retval false: 部分设备初始化失败
 * @note   会依次初始化蜂鸣器、RGB LED、电机
 * @note   即使部分设备失败，其他设备仍可正常使用
 * @note   初始化成功后会播放启动音效（如果蜂鸣器可用）
 * @note   应在 FreeRTOS 任务启动前调用（建议在 main.c 或 sensor_app.c 中）
 */
bool Drivers_Manager_Init(void);

/**
 * @brief  获取所有设备的初始化状态
 * @retval Drivers_Status_t: 包含各设备就绪标志的结构体
 * @note   可用于调试或 UI 显示设备状态
 */
Drivers_Status_t Drivers_Manager_GetStatus(void);

/* ==================== 蜂鸣器控制接口 ==================== */

/**
 * @brief  打开蜂鸣器并设置频率
 * @param  freq_hz: 目标频率 (Hz)，0 表示关闭
 * @note   仅在蜂鸣器初始化成功时生效
 * @note   持续发声，直到调用 Drivers_Buzzer_Off() 或设置频率为 0
 * @see    Buzzer_SetFrequency
 */
void Drivers_Buzzer_On(uint16_t freq_hz);

/**
 * @brief  关闭蜂鸣器
 * @note   仅在蜂鸣器初始化成功时生效
 * @see    Buzzer_Stop
 */
void Drivers_Buzzer_Off(void);

/**
 * @brief  播放简短的哔声（1000Hz，100ms）
 * @note   ⚠️ 使用 osDelay 实现延时，仅能在任务上下文调用
 * @note   适用于按键反馈、操作确认等场景
 * @see    Buzzer_Beep
 */
void Drivers_Buzzer_Beep(void);

/**
 * @brief  查询蜂鸣器是否正在播放
 * @retval true: 正在播放
 * @retval false: 已停止或蜂鸣器未初始化
 * @see    Buzzer_IsPlaying
 */
bool Drivers_Buzzer_IsPlaying(void);

/* ==================== RGB LED 控制接口 ==================== */

/**
 * @brief  关闭 RGB LED
 * @note   仅在 RGB LED 初始化成功时生效
 * @note   等效于 Drivers_RGBLED_SetColor(COLOR_OFF)
 * @see    RGB_LED_Off
 */
void Drivers_RGBLED_Off(void);

/**
 * @brief  设置 RGB LED 颜色
 * @param  color: RGB_Color 结构体，包含 R/G/B 值（0-255）
 * @note   仅在 RGB LED 初始化成功时生效
 * @note   可使用预定义颜色常量（如 COLOR_RED, COLOR_GREEN）
 * @note   示例：
 *         @code
 *         Drivers_RGBLED_SetColor(COLOR_RED);      // 红色
 *         Drivers_RGBLED_SetColor((RGB_Color){128, 0, 255}); // 自定义紫色
 *         @endcode
 * @see    RGB_LED_SetColorStruct
 */
void Drivers_RGBLED_SetColor(RGB_Color color);

/**
 * @brief  获取当前 RGB LED 颜色
 * @retval RGB_Color: 当前颜色值，未初始化时返回 COLOR_OFF
 * @note   返回的是原始颜色值（未经亮度缩放）
 * @see    RGB_LED_GetCurrentColor
 */
RGB_Color Drivers_RGBLED_GetColor(void);

/**
 * @brief  设置 LED 颜色槽位的自定义颜色
 * @param  slot: 槽位索引（1-3）
 * @param  color: RGB_Color 结构体
 * @retval true: 设置成功
 * @retval false: 槽位索引无效
 * @note   设置后立即生效（如果当前处于该槽位）
 * @note   颜色会保存到内存，但不会自动持久化
 * @note   示例：
 *         @code
 *         Drivers_RGBLED_SetSlotColor(1, COLOR_PURPLE);  // 槽位1设为紫色
 *         Drivers_RGBLED_SetSlotColor(2, (RGB_Color){255, 128, 0}); // 槽位2设为橙色
 *         @endcode
 */
bool Drivers_RGBLED_SetSlotColor(uint8_t slot, RGB_Color color);

/**
 * @brief  获取指定槽位的颜色
 * @param  slot: 槽位索引（1-3）
 * @param  color: 输出参数，用于存储颜色值
 * @retval true: 获取成功
 * @retval false: 槽位索引无效
 */
bool Drivers_RGBLED_GetSlotColor(uint8_t slot, RGB_Color *color);

/**
 * @brief  设置 RGB LED 亮度
 * @param  brightness: 亮度值（0-255），0=关闭，255=最亮
 * @note   仅在 RGB LED 初始化成功时生效
 * @note   会立即应用到当前颜色
 * @see    RGB_LED_SetBrightness
 */
void Drivers_RGBLED_SetBrightness(uint8_t brightness);

/* ==================== RGB LED 扩展控制接口 ==================== */

/**
 * @brief  设置 LED 控制模式
 * @param  mode: 控制模式
 *               - LED_MODE_MANUAL: 手动模式（UI控制颜色）
 *               - LED_MODE_AUTO: 自动模式（根据光照调节）
 * @note   仅在 RGB LED 初始化成功时生效
 */
void Drivers_RGBLED_SetMode(led_control_mode_t mode);

/**
 * @brief  获取当前 LED 控制模式
 * @retval led_control_mode_t: 当前模式
 */
led_control_mode_t Drivers_RGBLED_GetMode(void);

/**
 * @brief  手动循环切换 LED 颜色（手动模式专用）
 * @note   按顺序切换：白色 → 黄色 → 暖白色 → 关闭 → 白色...
 * @note   ⚠️ 仅在手动模式下生效
 */
void Drivers_RGBLED_CycleColor(void);

/**
 * @brief  获取当前手动模式的颜色状态
 * @retval led_manual_state_t: 当前状态
 */
led_manual_state_t Drivers_RGBLED_GetManualState(void);

/**
 * @brief  根据光照值自动调节 LED（自动模式专用）
 * @param  lux: 光照强度（来自 GY30 传感器）
 * @note   ⚠️ 仅在自动模式下生效
 * @note   应在数据更新定时器中调用
 * @note   调节规则：
 *         - lux < 50:  最亮暖白色（亮度 255）
 *         - 50-200:    中等亮度（亮度 150）
 *         - 200-500:   低亮度（亮度 80）
 *         - lux > 500: 关闭
 */
void Drivers_RGBLED_AutoAdjust(float lux);

/**
 * @brief 设置手动模式下的当前槽位
 * @param slot 槽位编号 (1-3)
 * @note 仅在手动模式下生效
 */
void Drivers_RGBLED_SetManualSlot(uint8_t slot);

/* ==================== 电机控制接口 ==================== */

/**
 * @brief  设置电机控制模式
 * @param  mode: 控制模式
 *               - MOTOR_MODE_AUTO: 速度由电位器 ADC 值决定
 *               - MOTOR_MODE_MANUAL: 速度由 Drivers_Motor_SetSpeed() 设置
 * @note   仅在电机初始化成功时生效
 * @note   切换到自动模式时会立即根据电位器值更新速度
 * @see    Motor_SetControlMode
 */
void Drivers_Motor_SetMode(Motor_Control_Mode_t mode);

/**
 * @brief  获取当前电机控制模式
 * @retval Motor_Control_Mode_t: 当前模式，未初始化时返回 MOTOR_MODE_MANUAL
 * @see    Motor_GetControlMode
 */
Motor_Control_Mode_t Drivers_Motor_GetMode(void);

/**
 * @brief  手动设置电机速度
 * @param  speed: PWM 占空比（0-999），0=停止，999=最大速度
 * @note   ⚠️ 仅在手动模式（MOTOR_MODE_MANUAL）下生效
 * @note   在自动模式下调用无效（会被电位器值覆盖）
 * @see    Motor_SetSpeed
 */
void Drivers_Motor_SetSpeed(uint16_t speed);

/**
 * @brief  获取电机当前速度
 * @retval uint16_t: 当前 PWM 占空比（0-999），未初始化时返回 0
 * @note   可用于 UI 显示或调试
 * @see    Motor_GetCurrentSpeed
 */
uint16_t Drivers_Motor_GetSpeed(void);

/**
 * @brief  获取电位器 ADC 原始值
 * @retval uint16_t: 电位器 ADC 值（0-4095），未初始化时返回 0
 * @note   对应 12 位 ADC 的采样范围
 * @note   可用于显示电位器位置或调试自动模式
 * @see    Motor_GetPotValue
 */
uint16_t Drivers_Motor_GetPotValue(void);

/* ==================== 周期性维护接口 ==================== */

/**
 * @brief  周期性更新设备状态
 * @note   应在 FreeRTOS 任务中以 50-100ms 周期调用
 * @note   当前功能：
 *         - 更新电机速度（自动模式下根据电位器值调速）
 *         - 预留其他设备的周期性任务接口
 * @note   示例：
 *         @code
 *         void VerificationTask_Entry(void const *argument)
 *         {
 *             for(;;)
 *             {
 *                 Drivers_Manager_Update();
 *                 osDelay(100);
 *             }
 *         }
 *         @endcode
 */
void Drivers_Manager_Update(void);

#ifdef __cplusplus
}
#endif

#endif // __DEVICES_MANAGER_H
