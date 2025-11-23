/**
 ******************************************************************************
 * @file    rgbled.h
 * @brief   RGB LED 驱动头文件
 * @details 提供基于 PWM 的 RGB LED 颜色控制、亮度调节、HSV 转换等功能
 * @author  MmsY
 * @date    2025
 ******************************************************************************
 */

#ifndef __RGB_LED_H
#define __RGB_LED_H

#include "main.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 数据结构定义 ==================== */

/**
 * @brief RGB 颜色结构体
 * @note  每个通道取值范围 0-255
 */
typedef struct {
    uint8_t R;  /**< 红色通道 (0-255) */
    uint8_t G;  /**< 绿色通道 (0-255) */
    uint8_t B;  /**< 蓝色通道 (0-255) */
} RGB_Color;

/**
 * @brief RGB LED 驱动状态枚举
 */
typedef enum {
    RGB_LED_OK = 0,     /**< 操作成功 */
    RGB_LED_ERROR       /**< 操作失败 */
} RGB_LED_Status_t;

/* ==================== 基础控制函数 ==================== */

/**
 * @brief  初始化 RGB LED
 * @retval RGB_LED_OK: 初始化成功
 * @retval RGB_LED_ERROR: PWM 启动失败
 * @note   会启动 TIM2 的三个 PWM 通道（CH1/CH2/CH3 对应 R/G/B）
 * @note   初始化后 LED 默认关闭
 */
RGB_LED_Status_t RGB_LED_Init(void);

/**
 * @brief  设置 RGB LED 颜色
 * @param  r: 红色分量 (0-255)
 * @param  g: 绿色分量 (0-255)
 * @param  b: 蓝色分量 (0-255)
 * @note   实际输出会根据全局亮度设置进行缩放
 * @note   支持共阳/共阴 LED（通过 LED_TYPE_COMMON_ANODE 宏控制）
 */
void RGB_LED_SetColor(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief  使用颜色结构体设置 RGB LED
 * @param  color: RGB_Color 结构体，包含 R/G/B 三个通道值
 * @note   等效于 RGB_LED_SetColor(color.R, color.G, color.B)
 * @see    RGB_LED_SetColor
 */
void RGB_LED_SetColorStruct(RGB_Color color);

/**
 * @brief  设置 RGB LED 全局亮度
 * @param  brightness: 亮度值 (0-255)，0=关闭，255=最亮
 * @note   会立即应用到当前颜色，实现亮度调节而不改变色调
 * @note   内部通过缩放 R/G/B 值实现
 */
void RGB_LED_SetBrightness(uint8_t brightness);

/**
 * @brief  关闭 RGB LED
 * @note   等效于 RGB_LED_SetColor(0, 0, 0)
 */
void RGB_LED_Off(void);

/**
 * @brief  获取当前 RGB LED 颜色
 * @retval RGB_Color 结构体，包含当前设置的 R/G/B 值（未缩放）
 * @note   返回的是原始颜色值，不包含亮度缩放影响
 */
RGB_Color RGB_LED_GetCurrentColor(void);

/**
 * @brief  获取当前全局亮度
 * @retval 亮度值 (0-255)
 */
uint8_t RGB_LED_GetBrightness(void);

/* ==================== 颜色转换函数 ==================== */

/**
 * @brief  HSV 色彩空间转 RGB 色彩空间
 * @param  h: 色相 (0-359 度)
 * @param  s: 饱和度 (0-255)
 * @param  v: 明度 (0-255)
 * @retval RGB_Color 结构体
 * @note   用于实现彩虹渐变、呼吸灯等效果
 * @note   算法基于标准 HSV 到 RGB 转换公式
 */
RGB_Color HSV_to_RGB(uint16_t h, uint8_t s, uint8_t v);

/* ==================== 预定义颜色常量 ==================== */

/**
 * @defgroup RGB_Predefined_Colors 预定义常用颜色
 * @{
 */
#define COLOR_RED           (RGB_Color){255, 0, 0}    /**< 红色 */
#define COLOR_GREEN         (RGB_Color){0, 255, 0}    /**< 绿色 */
#define COLOR_BLUE          (RGB_Color){0, 0, 255}    /**< 蓝色 */
#define COLOR_WHITE         (RGB_Color){255, 255, 255}/**< 白色 */
#define COLOR_YELLOW        (RGB_Color){255, 255, 0}  /**< 黄色 */
#define COLOR_WARM_WHITE    (RGB_Color){255, 204, 153}/**< 暖白色 */
#define COLOR_CYAN          (RGB_Color){0, 255, 255}  /**< 青色 */
#define COLOR_MAGENTA       (RGB_Color){255, 0, 255}  /**< 洋红色 */
#define COLOR_ORANGE        (RGB_Color){255, 165, 0}  /**< 橙色 */
#define COLOR_PURPLE        (RGB_Color){128, 0, 128}  /**< 紫色 */
#define COLOR_PINK          (RGB_Color){255, 192, 203}/**< 粉色 */
#define COLOR_OFF           (RGB_Color){0, 0, 0}      /**< 关闭 */
/** @} */

#ifdef __cplusplus
}
#endif

#endif // __RGB_LED_H
