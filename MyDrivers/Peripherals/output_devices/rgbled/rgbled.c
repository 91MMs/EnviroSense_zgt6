/**
 ******************************************************************************
 * @file    rgbled.c
 * @brief   RGB LED 驱动实现
 * @details 基于 STM32 TIM2 PWM 输出控制 RGB LED 三通道
 *          支持颜色设置、亮度调节、HSV 转换等功能
 * @author  MmsY
 * @date    2025
 ******************************************************************************
 */

#include "rgbled.h"
#include "tim.h"

#define LOG_MODULE "RGB_LED"
#include "log.h"

/* ==================== 硬件配置 ==================== */

/**
 * @brief LED 类型定义
 * @note  0 = 共阴极（低电平点亮）
 * @note  1 = 共阳极（高电平点亮，需要反相 PWM）
 */
#define LED_TYPE_COMMON_ANODE 0

/* ==================== 静态变量 ==================== */

/**
 * @brief 当前颜色缓存（原始值，未经亮度缩放）
 */
static RGB_Color s_current_color = {0, 0, 0};

/**
 * @brief 全局亮度值 (0-255)
 * @note  255 = 100% 亮度，0 = 关闭
 */
static uint8_t s_global_brightness = 255;

/* ==================== 公共函数实现 ==================== */

/**
 * @brief 初始化 RGB LED
 */
RGB_LED_Status_t RGB_LED_Init(void)
{
    /* 启动 TIM2 的三个 PWM 通道 */
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK ||  /* R 通道 */
        HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2) != HAL_OK ||  /* G 通道 */
        HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3) != HAL_OK) {  /* B 通道 */
        LOG_ERROR("RGB LED PWM启动失败");
        return RGB_LED_ERROR;
    }
    
    /* 初始状态设为关闭 */
    RGB_LED_Off();
    LOG_INFO("RGB LED初始化完成");
    return RGB_LED_OK;
}

/**
 * @brief 设置 RGB 颜色
 */
void RGB_LED_SetColor(uint8_t r, uint8_t g, uint8_t b)
{
    /* 保存原始颜色值（供后续亮度调节使用） */
    s_current_color.R = r;
    s_current_color.G = g;
    s_current_color.B = b;
    
    /* 应用全局亮度缩放（避免浮点运算，使用整数除法） */
    uint16_t red   = (r * s_global_brightness) / 255;
    uint16_t green = (g * s_global_brightness) / 255;
    uint16_t blue  = (b * s_global_brightness) / 255;
    
    /* 获取 PWM 最大计数值（ARR 寄存器值） */
    uint16_t max_pwm = __HAL_TIM_GET_AUTORELOAD(&htim2);
    
    /* 将 0-255 颜色值映射到 PWM 占空比 */
    uint16_t pwm_r = (red   * max_pwm) / 255;
    uint16_t pwm_g = (green * max_pwm) / 255;
    uint16_t pwm_b = (blue  * max_pwm) / 255;
    
    /* 如果是共阳极 LED，需要反相 PWM（高电平=灭，低电平=亮） */
    if (LED_TYPE_COMMON_ANODE) {
        pwm_r = max_pwm - pwm_r;
        pwm_g = max_pwm - pwm_g;
        pwm_b = max_pwm - pwm_b;
    }
    
    /* 更新 PWM 比较寄存器（立即生效） */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pwm_r);  /* R 通道 */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pwm_g);  /* G 通道 */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pwm_b);  /* B 通道 */
}

/**
 * @brief 使用颜色结构体设置
 */
void RGB_LED_SetColorStruct(RGB_Color color)
{
    RGB_LED_SetColor(color.R, color.G, color.B);
}

/**
 * @brief 设置亮度
 */
void RGB_LED_SetBrightness(uint8_t brightness)
{
    /* 更新全局亮度值 */
    s_global_brightness = brightness;
    
    /* 用新亮度重新设置当前颜色（实现亮度调节） */
    RGB_LED_SetColor(s_current_color.R, s_current_color.G, s_current_color.B);
}

/**
 * @brief 关闭 LED
 */
void RGB_LED_Off(void)
{
    RGB_LED_SetColor(0, 0, 0);
}

/**
 * @brief 获取当前颜色
 */
RGB_Color RGB_LED_GetCurrentColor(void)
{
    return s_current_color;
}

/**
 * @brief 获取当前亮度
 */
uint8_t RGB_LED_GetBrightness(void)
{
    return s_global_brightness;
}

/**
 * @brief HSV 转 RGB
 * @note  算法基于标准 HSV 色彩模型
 * @note  H(色相): 0-359 度，分为 6 个区间（红-黄-绿-青-蓝-洋红）
 * @note  S(饱和度): 0=灰色，255=纯色
 * @note  V(明度): 0=黑色，255=最亮
 */
RGB_Color HSV_to_RGB(uint16_t h, uint8_t s, uint8_t v)
{
    RGB_Color rgb;
    
    /* 特殊情况：饱和度为 0 时，输出灰度 */
    if (s == 0) {
        rgb.R = rgb.G = rgb.B = v;
        return rgb;
    }
    
    /* 计算色相所在的色轮区间 (0-5) */
    uint8_t region = h / 60;
    
    /* 计算区间内的偏移量（0-255 范围内） */
    uint8_t remainder = (h % 60) * 6;
    
    /* 计算三个中间值（使用位移避免浮点运算） */
    uint8_t p = (v * (255 - s)) >> 8;                           /* 最小亮度 */
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;       /* 递减值 */
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8; /* 递增值 */
    
    /* 根据色相区间分配 RGB 值 */
    switch (region) {
        case 0: rgb.R = v; rgb.G = t; rgb.B = p; break;  /* 红 -> 黄 */
        case 1: rgb.R = q; rgb.G = v; rgb.B = p; break;  /* 黄 -> 绿 */
        case 2: rgb.R = p; rgb.G = v; rgb.B = t; break;  /* 绿 -> 青 */
        case 3: rgb.R = p; rgb.G = q; rgb.B = v; break;  /* 青 -> 蓝 */
        case 4: rgb.R = t; rgb.G = p; rgb.B = v; break;  /* 蓝 -> 洋红 */
        default: rgb.R = v; rgb.G = p; rgb.B = q; break; /* 洋红 -> 红 */
    }
    
    return rgb;
}
