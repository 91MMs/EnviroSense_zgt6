/**
 ******************************************************************************
 * @file    buzzer.c
 * @brief   无源蜂鸣器驱动实现
 * @details 基于 STM32 TIM3 PWM 输出控制无源蜂鸣器频率
 *          支持音符播放、旋律播放、预定义音效等功能
 * @author  MmsY
 * @date    2025
 ******************************************************************************
 */

#include "buzzer.h"
#include "tim.h"
#include "cmsis_os.h"

#define LOG_MODULE "BUZZER"
#include "log.h"

/* ==================== 硬件配置 ==================== */

/**
 * @brief TIM3 时钟频率（APB1 Timer Clock = 84MHz）
 * @note  STM32F407 中 APB1 时钟倍频后为 84MHz
 */
#define TIM3_CLOCK_FREQ  84000000UL

/**
 * @brief 蜂鸣器频率范围限制
 * @note  过低频率人耳难以感知，过高频率蜂鸣器无法响应
 */
#define FREQ_MIN         100    /**< 最低频率 100Hz */
#define FREQ_MAX         5000   /**< 最高频率 5kHz */

/* ==================== 静态变量 ==================== */

/**
 * @brief 蜂鸣器播放状态标志
 * @note  true: 正在播放，false: 已停止
 */
static bool s_is_playing = false;

/* ==================== 公共函数实现 ==================== */

/**
 * @brief 初始化无源蜂鸣器
 */
Buzzer_Status_t Buzzer_Init(void)
{
    /* 停止 PWM 输出，确保初始状态为关闭 */
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    s_is_playing = false;
    LOG_INFO("蜂鸣器初始化完成");
    return BUZZER_OK;
}

/**
 * @brief 设置蜂鸣器频率
 * @param freq_hz: 目标频率 (Hz)，0 为关闭
 * 
 * @note 工作原理：
 *       PWM频率 = TIM_CLK / (PSC+1) / (ARR+1)
 *       固定 PSC=83（在 CubeMX 中配置），通过调整 ARR 改变频率
 *       ARR = TIM_CLK / (PSC+1) / freq - 1
 * 
 * @note 占空比固定为 50%（CCR = ARR/2），确保音量稳定
 */
void Buzzer_SetFrequency(uint16_t freq_hz)
{
    /* 频率为 0 时关闭蜂鸣器 */
    if (freq_hz == 0) {
        Buzzer_Stop();
        return;
    }
    
    /* 限制频率范围，避免硬件损坏或无效输出 */
    if (freq_hz < FREQ_MIN) freq_hz = FREQ_MIN;
    if (freq_hz > FREQ_MAX) freq_hz = FREQ_MAX;
    
    /* 读取当前预分频器值（PSC 寄存器） */
    uint16_t psc = htim3.Instance->PSC;
    
    /* 计算自动重装载值（ARR 寄存器）
     * 公式：ARR = TIM_CLK / (PSC+1) / freq - 1
     * 例：84MHz / (83+1) / 1000Hz - 1 = 999
     */
    uint32_t arr_value = (TIM3_CLOCK_FREQ / (psc + 1) / freq_hz) - 1;
    
    /* 限制 ARR 范围（16位定时器最大值 65535） */
    if (arr_value > 65535) arr_value = 65535;
    if (arr_value < 1) arr_value = 1;
    
    /* 停止 PWM 输出（避免修改寄存器时产生毛刺） */
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    
    /* 设置新的自动重装载值 */
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr_value);
    
    /* 设置 50% 占空比（CCR = ARR/2） */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, arr_value / 2);
    
    /* 产生更新事件，立即加载新的 ARR 和 CCR 值 */
    htim3.Instance->EGR = TIM_EGR_UG;
    
    /* 重新启动 PWM 输出 */
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    
    /* 更新播放状态 */
    s_is_playing = true;
}

/**
 * @brief 停止蜂鸣器
 */
void Buzzer_Stop(void)
{
    /* 停止 PWM 输出 */
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    s_is_playing = false;
}

/**
 * @brief 查询蜂鸣器是否正在播放
 */
bool Buzzer_IsPlaying(void)
{
    return s_is_playing;
}

/**
 * @brief 播放指定频率和时长的音调
 * @param freq_hz: 频率 (Hz)
 * @param duration_ms: 持续时间 (ms)
 * 
 * @note ⚠️ 此函数会阻塞当前任务，使用 osDelay 实现延时
 * @note 仅能在 FreeRTOS 任务上下文中调用，不可在中断中使用
 */
void Buzzer_PlayTone(uint16_t freq_hz, uint16_t duration_ms)
{
    Buzzer_SetFrequency(freq_hz);
    osDelay(duration_ms);  /* 阻塞等待 */
    Buzzer_Stop();
}

/**
 * @brief 播放音符
 * @param note: 音符结构体
 * 
 * @note 如果音符频率为 NOTE_SILENCE，则仅延时不发声（休止符）
 */
void Buzzer_PlayNote(Note note)
{
    if (note.freq == NOTE_SILENCE) {
        /* 休止符：停止发声，仅延时 */
        Buzzer_Stop();
        osDelay(note.duration);
    } else {
        /* 普通音符：播放指定频率和时长 */
        Buzzer_PlayTone(note.freq, note.duration);
    }
}

/**
 * @brief 播放旋律
 * @param melody: 音符数组
 * @param length: 音符数量
 * 
 * @note 每个音符之间会自动插入 50ms 间隔，增强节奏感
 */
void Buzzer_PlayMelody(const Note* melody, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++) {
        Buzzer_PlayNote(melody[i]);
        osDelay(50);  /* 音符间隔，避免音符粘连 */
    }
}

/* ==================== 预定义音效 ==================== */

/**
 * @brief 简单的哔声
 * @note  1000Hz，100ms，适用于按键反馈
 */
void Buzzer_Beep(void)
{
    Buzzer_PlayTone(1000, 100);
}

/**
 * @brief 开机音效（上升音阶）
 * @note  播放 Do-Mi-Sol，总时长约 400ms
 */
void Buzzer_StartupSound(void)
{
    const Note startup[] = {
        {NOTE_M1, 100},  /* Do (523Hz) */
        {NOTE_M3, 100},  /* Mi (659Hz) */
        {NOTE_M5, 200}   /* Sol (784Hz) */
    };
    Buzzer_PlayMelody(startup, 3);
}

/**
 * @brief 成功音效（上升后稳定）
 * @note  播放 Sol-高Do-高Mi，愉悦音调
 */
void Buzzer_SuccessSound(void)
{
    const Note success[] = {
        {NOTE_M5, 100},  /* Sol (784Hz) */
        {NOTE_H1, 100},  /* 高Do (1047Hz) */
        {NOTE_H3, 300}   /* 高Mi (1319Hz) */
    };
    Buzzer_PlayMelody(success, 3);
}

/**
 * @brief 错误音效（下降音阶）
 * @note  播放 Sol-Mi-Do，提示操作失败
 */
void Buzzer_ErrorSound(void)
{
    const Note error[] = {
        {NOTE_M5, 150},  /* Sol (784Hz) */
        {NOTE_M3, 150},  /* Mi (659Hz) */
        {NOTE_M1, 300}   /* Do (523Hz) */
    };
    Buzzer_PlayMelody(error, 3);
}

/**
 * @brief 警告音效（交替音）
 * @note  800Hz 和 1200Hz 快速交替，重复 3 次
 * @note  总时长约 900ms，适用于紧急报警
 */
void Buzzer_WarningSound(void)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        Buzzer_PlayTone(800, 100);   /* 低音 */
        osDelay(50);                 /* 短暂停顿 */
        Buzzer_PlayTone(1200, 100);  /* 高音 */
        osDelay(50);                 /* 短暂停顿 */
    }
}
