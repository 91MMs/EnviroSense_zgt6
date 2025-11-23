/**
 ******************************************************************************
 * @file    devices_manager.c
 * @brief   外设设备统一管理器实现（整理版）
 * @details 负责初始化和管理蜂鸣器、RGB LED、电机三种外设
 *          为上层应用提供统一的控制接口，隐藏底层驱动细节
 ******************************************************************************
 */

#include "devices_manager.h"

/* ==================== 静态变量 ==================== */

static Drivers_Status_t s_status = {false, false, false};

/* RGB LED 状态管理 */
static led_control_mode_t s_led_mode = LED_MODE_MANUAL;
static led_manual_state_t s_led_manual_state = LED_STATE_OFF;
static RGB_Color s_led_color_slots[3] = {
    {255, 0, 0},      // 槽位1默认：红色
    {0, 255, 0},      // 槽位2默认：绿色
    {0, 0, 255}       // 槽位3默认：蓝色
};

/* [SIMPLIFIED] 自动调光固定配置 */
#define AUTO_LUX_MIN            50.0f    // 低于此值亮度固定为 255
#define AUTO_LUX_MAX            1500.0f   // 高于此值关闭 LED
#define AUTO_BRIGHTNESS_MIN     40       // 映射范围最小亮度

/* ==================== 蜂鸣器控制接口 ==================== */

void Drivers_Buzzer_On(uint16_t freq_hz)
{
    if (s_status.buzzer_ready) {
        Buzzer_SetFrequency(freq_hz);
    }
}

void Drivers_Buzzer_Off(void)
{
    if (s_status.buzzer_ready) {
        Buzzer_Stop();
    }
}

void Drivers_Buzzer_Beep(void)
{
    if (s_status.buzzer_ready) {
        Buzzer_Beep();
    }
}

bool Drivers_Buzzer_IsPlaying(void)
{
    return s_status.buzzer_ready ? Buzzer_IsPlaying() : false;
}

/* ==================== RGB LED 控制接口 ==================== */
/* 线性映射函数：将 lux 映射到亮度值 */
static uint8_t map_lux_to_brightness(float lux)
{
    if (lux <= AUTO_LUX_MIN) {
        /* 极暗环境：最大亮度 */
        return 255;
    }
    
    if (lux >= AUTO_LUX_MAX) {
        /* 明亮环境：关闭 LED */
        return 0;
    }
    
    /* 中间区域：线性映射（光照越高，亮度越低）*/
    float lux_range = AUTO_LUX_MAX - AUTO_LUX_MIN;
    float brightness_range = 255.0f - AUTO_BRIGHTNESS_MIN;
    
    /* 计算归一化位置（0.0 ~ 1.0） */
    float normalized = (lux - AUTO_LUX_MIN) / lux_range;
    
    /* 反向映射（lux 增加时亮度降低） */
    uint8_t brightness = 255 - (uint8_t)(normalized * brightness_range);
    
    /* 确保在范围内 */
    if (brightness < AUTO_BRIGHTNESS_MIN) {
        brightness = AUTO_BRIGHTNESS_MIN;
    }
    
    return brightness;
}

/* 设置槽位颜色 */
bool Drivers_RGBLED_SetSlotColor(uint8_t slot, RGB_Color color)
{
    if (slot < 1 || slot > 3) {
        return false;
    }
    
    s_led_color_slots[slot - 1] = color;
    
    /* 如果当前处于该槽位，立即应用新颜色 */
    if (s_led_mode == LED_MODE_MANUAL) {
        if ((slot == 1 && s_led_manual_state == LED_STATE_SLOT_1) ||
            (slot == 2 && s_led_manual_state == LED_STATE_SLOT_2) ||
            (slot == 3 && s_led_manual_state == LED_STATE_SLOT_3)) {
            
            if (s_status.rgb_led_ready) {
                RGB_LED_SetColorStruct(color);
            }
        }
    } else if (s_led_mode == LED_MODE_AUTO && slot == 1) {
        /* 自动模式下只更新槽位1 */
        if (s_status.rgb_led_ready) {
            RGB_LED_SetColorStruct(color);
        }
    }
    
    return true;
}

/* 获取槽位颜色 */
bool Drivers_RGBLED_GetSlotColor(uint8_t slot, RGB_Color *color)
{
    if (slot < 1 || slot > 3 || color == NULL) {
        return false;
    }
    
    *color = s_led_color_slots[slot - 1];
    return true;
}

/* 重置槽位颜色为默认值（红/绿/蓝） */
void Drivers_RGBLED_ResetSlotColors(void)
{
    s_led_color_slots[0] = (RGB_Color){255, 0, 0};      // 槽位1：红色
    s_led_color_slots[1] = (RGB_Color){0, 255, 0};      // 槽位2：绿色
    s_led_color_slots[2] = (RGB_Color){0, 0, 255};      // 槽位3：蓝色
}

/* 设置 LED 控制模式（自动/手动） */
void Drivers_RGBLED_SetMode(led_control_mode_t mode)
{
    if (!s_status.rgb_led_ready) {
        return;
    }
    
    s_led_mode = mode;
    
    if (mode == LED_MODE_MANUAL) {
        /* 手动模式：恢复上次的槽位状态 */
        switch (s_led_manual_state) {
            case LED_STATE_SLOT_1:
                RGB_LED_SetColorStruct(s_led_color_slots[0]);
                break;
            case LED_STATE_SLOT_2:
                RGB_LED_SetColorStruct(s_led_color_slots[1]);
                break;
            case LED_STATE_SLOT_3:
                RGB_LED_SetColorStruct(s_led_color_slots[2]);
                break;
            case LED_STATE_OFF:
            default:
                RGB_LED_Off();
                break;
        }
    } else {
        /* 自动模式：默认使用槽位1的颜色 */
        s_led_manual_state = LED_STATE_SLOT_1;
        RGB_LED_SetColorStruct(s_led_color_slots[0]);
    }
}

/* 获取当前 LED 控制模式 */
led_control_mode_t Drivers_RGBLED_GetMode(void)
{
    return s_led_mode;
}

/* 获取当前手动模式状态 */
led_manual_state_t Drivers_RGBLED_GetManualState(void)
{
    return s_led_manual_state;
}

/* 设置手动模式下的当前槽位 */
void Drivers_RGBLED_SetManualSlot(uint8_t slot)
{
    if (!s_status.rgb_led_ready) {
        return;
    }
    
    if (s_led_mode != LED_MODE_MANUAL) {
        return;
    }
    
    if (slot < 1 || slot > 3) {
        return;
    }
    
    switch (slot) {
        case 1:
            s_led_manual_state = LED_STATE_SLOT_1;
            RGB_LED_SetColorStruct(s_led_color_slots[0]);
            break;
            
        case 2:
            s_led_manual_state = LED_STATE_SLOT_2;
            RGB_LED_SetColorStruct(s_led_color_slots[1]);
            break;
            
        case 3:
            s_led_manual_state = LED_STATE_SLOT_3;
            RGB_LED_SetColorStruct(s_led_color_slots[2]);
            break;
            
        default:
            break;
    }
}

/* 根据光照值自动调节 LED（仅自动模式下生效） */
void Drivers_RGBLED_AutoAdjust(float lux)
{
    if (!s_status.rgb_led_ready) {
        return;
    }
    
    if (s_led_mode != LED_MODE_AUTO) {
        return;
    }
    
    /* 使用映射函数计算亮度 */
    uint8_t brightness = map_lux_to_brightness(lux);
    
    if (brightness == 0) {
        /* 明亮环境：关闭 LED */
        RGB_LED_Off();
    } else {
        /* 暗环境：显示槽位1的颜色，调节亮度 */
        RGB_LED_SetColorStruct(s_led_color_slots[0]);
        RGB_LED_SetBrightness(brightness);
    }
}

/* 直接设置 RGB LED 颜色（不影响槽位） */
void Drivers_RGBLED_SetColor(RGB_Color color)
{
    if (s_status.rgb_led_ready) {
        RGB_LED_SetColorStruct(color);
    }
}

/* 关闭 RGB LED */
void Drivers_RGBLED_Off(void)
{
    if (s_status.rgb_led_ready) {
        RGB_LED_Off();
        
        /* 在手动模式下，更新状态为关闭 */
        if (s_led_mode == LED_MODE_MANUAL) {
            s_led_manual_state = LED_STATE_OFF;
        }
    }
}

/* 手动循环切换 LED 颜色（槽位1 → 槽位2 → 槽位3 → 关闭 → 槽位1...） */
void Drivers_RGBLED_CycleColor(void)
{
    if (!s_status.rgb_led_ready) {
        return;
    }
    
    if (s_led_mode != LED_MODE_MANUAL) {
        return;
    }
    
    switch (s_led_manual_state) {
        case LED_STATE_OFF:
            s_led_manual_state = LED_STATE_SLOT_1;
            RGB_LED_SetColorStruct(s_led_color_slots[0]);
            break;
            
        case LED_STATE_SLOT_1:
            s_led_manual_state = LED_STATE_SLOT_2;
            RGB_LED_SetColorStruct(s_led_color_slots[1]);
            break;
            
        case LED_STATE_SLOT_2:
            s_led_manual_state = LED_STATE_SLOT_3;
            RGB_LED_SetColorStruct(s_led_color_slots[2]);
            break;
            
        case LED_STATE_SLOT_3:
            s_led_manual_state = LED_STATE_OFF;
            RGB_LED_Off();
            break;
            
        default:
            s_led_manual_state = LED_STATE_OFF;
            RGB_LED_Off();
            break;
    }
}

/* 获取当前 RGB LED 颜色 */
RGB_Color Drivers_RGBLED_GetColor(void)
{
    return s_status.rgb_led_ready ? RGB_LED_GetCurrentColor() : COLOR_OFF;
}

/* 设置 RGB LED 亮度 */
void Drivers_RGBLED_SetBrightness(uint8_t brightness)
{
    if (s_status.rgb_led_ready) {
        RGB_LED_SetBrightness(brightness);
    }
}

/* ==================== 电机控制接口 ==================== */

void Drivers_Motor_SetMode(Motor_Control_Mode_t mode)
{
    if (s_status.motor_ready) {
        Motor_SetControlMode(mode);
    }
}

Motor_Control_Mode_t Drivers_Motor_GetMode(void)
{
    return s_status.motor_ready ? Motor_GetControlMode() : MOTOR_MODE_MANUAL;
}

void Drivers_Motor_SetSpeed(uint16_t speed)
{
    if (s_status.motor_ready) {
        Motor_SetSpeed(speed);
    }
}

uint16_t Drivers_Motor_GetSpeed(void)
{
    return s_status.motor_ready ? Motor_GetCurrentSpeed() : 0;
}

uint16_t Drivers_Motor_GetPotValue(void)
{
    return s_status.motor_ready ? Motor_GetPotValue() : 0;
}

/* ==================== 系统管理接口 ==================== */

bool Drivers_Manager_Init(void)
{
    /* 初始化蜂鸣器 */
    s_status.buzzer_ready = (Buzzer_Init() == BUZZER_OK);
    
    /* 初始化 RGB LED */
    s_status.rgb_led_ready = (RGB_LED_Init() == RGB_LED_OK);
    
    /* 初始化电机控制 */
    s_status.motor_ready = (Motor_Init() == MOTOR_OK);
    
    /* 检查总体状态 */
    bool all_ok = s_status.buzzer_ready && 
                  s_status.rgb_led_ready && 
                  s_status.motor_ready;
    
    if (all_ok && s_status.buzzer_ready) {
        Buzzer_StartupSound();
    }
    
    return all_ok;
}

Drivers_Status_t Drivers_Manager_GetStatus(void)
{
    return s_status;
}

void Drivers_Manager_Update(void)
{
    if (s_status.motor_ready) {
        Motor_Update();
    }
}
