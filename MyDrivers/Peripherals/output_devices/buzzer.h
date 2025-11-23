/**
 ******************************************************************************
 * @file    buzzer.h
 * @brief   无源蜂鸣器驱动头文件
 * @details 提供基于 PWM 的无源蜂鸣器频率控制、音符播放、旋律播放等功能
 * @author  MmsY
 * @date    2025
 ******************************************************************************
 */

#ifndef __BUZZER_H
#define __BUZZER_H

#include "main.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 数据结构定义 ==================== */

/**
 * @brief 音符频率定义（标准音阶）
 * @note  基于国际标准音高 A4 = 440Hz
 * @note  频率单位：Hz
 */
typedef enum {
    NOTE_SILENCE = 0,   /**< 静音 */
    
    /* 低音组（3组，C4-B4） */
    NOTE_L1 = 262,      /**< Do (C4) */
    NOTE_L2 = 294,      /**< Re (D4) */
    NOTE_L3 = 330,      /**< Mi (E4) */
    NOTE_L4 = 349,      /**< Fa (F4) */
    NOTE_L5 = 392,      /**< Sol (G4) */
    NOTE_L6 = 440,      /**< La (A4) - 标准音 */
    NOTE_L7 = 494,      /**< Si (B4) */
    
    /* 中音组（4组，C5-B5） */
    NOTE_M1 = 523,      /**< Do (C5) - 中央C */
    NOTE_M2 = 587,      /**< Re (D5) */
    NOTE_M3 = 659,      /**< Mi (E5) */
    NOTE_M4 = 698,      /**< Fa (F5) */
    NOTE_M5 = 784,      /**< Sol (G5) */
    NOTE_M6 = 880,      /**< La (A5) */
    NOTE_M7 = 988,      /**< Si (B5) */
    
    /* 高音组（5组，C6-B6） */
    NOTE_H1 = 1047,     /**< Do (C6) */
    NOTE_H2 = 1175,     /**< Re (D6) */
    NOTE_H3 = 1319,     /**< Mi (E6) */
    NOTE_H4 = 1397,     /**< Fa (F6) */
    NOTE_H5 = 1568,     /**< Sol (G6) */
    NOTE_H6 = 1760,     /**< La (A6) */
    NOTE_H7 = 1976      /**< Si (B6) */
} NoteFreq;

/**
 * @brief 音符结构体
 * @note  用于定义旋律中的单个音符
 */
typedef struct {
    uint16_t freq;      /**< 频率 (Hz)，可使用 NoteFreq 枚举值 */
    uint16_t duration;  /**< 持续时间 (ms) */
} Note;

/**
 * @brief 蜂鸣器驱动状态枚举
 */
typedef enum {
    BUZZER_OK = 0,      /**< 操作成功 */
    BUZZER_ERROR        /**< 操作失败 */
} Buzzer_Status_t;

/* ==================== 基础控制函数 ==================== */

/**
 * @brief  初始化无源蜂鸣器
 * @retval BUZZER_OK: 初始化成功
 * @retval BUZZER_ERROR: 初始化失败
 * @note   会停止 TIM3_CH1 的 PWM 输出，初始状态为关闭
 * @note   蜂鸣器需通过 PWM 频率变化产生不同音调
 */
Buzzer_Status_t Buzzer_Init(void);

/**
 * @brief  设置蜂鸣器输出频率
 * @param  freq_hz: 目标频率 (Hz)，0 表示关闭
 * @note   频率范围限制：100Hz - 5000Hz
 * @note   通过动态调整 TIM3 的 ARR 寄存器实现频率控制
 * @note   固定占空比 50%，确保音量稳定
 * @note   工作原理：PWM频率 = TIM_CLK / (PSC+1) / (ARR+1)
 */
void Buzzer_SetFrequency(uint16_t freq_hz);

/**
 * @brief  停止蜂鸣器输出
 * @note   关闭 PWM 输出，蜂鸣器停止发声
 */
void Buzzer_Stop(void);

/**
 * @brief  查询蜂鸣器是否正在播放
 * @retval true: 正在播放
 * @retval false: 已停止
 */
bool Buzzer_IsPlaying(void);

/* ==================== 播放函数 ==================== */

/**
 * @brief  播放指定频率和时长的音调
 * @param  freq_hz: 频率 (Hz)
 * @param  duration_ms: 持续时间 (ms)
 * @note   ?? 使用 osDelay 实现延时，会阻塞当前任务
 * @note   ?? 仅能在 FreeRTOS 任务上下文中调用，不可在中断中使用
 * @note   播放完成后会自动停止
 */
void Buzzer_PlayTone(uint16_t freq_hz, uint16_t duration_ms);

/**
 * @brief  播放单个音符
 * @param  note: 音符结构体，包含频率和时长
 * @note   ?? 使用 osDelay 实现延时，仅能在任务上下文调用
 * @note   支持静音符号（NOTE_SILENCE）
 */
void Buzzer_PlayNote(Note note);

/**
 * @brief  播放旋律（音符序列）
 * @param  melody: 音符数组指针
 * @param  length: 音符数量
 * @note   ?? 使用 osDelay 实现延时，仅能在任务上下文调用
 * @note   音符间会自动插入 50ms 间隔，增强节奏感
 * @note   示例：
 *         @code
 *         const Note melody[] = {
 *             {NOTE_M1, 200}, {NOTE_M2, 200}, {NOTE_M3, 400}
 *         };
 *         Buzzer_PlayMelody(melody, 3);
 *         @endcode
 */
void Buzzer_PlayMelody(const Note* melody, uint16_t length);

/* ==================== 预定义音效 ==================== */

/**
 * @defgroup Buzzer_Predefined_Sounds 预定义音效函数
 * @{
 */

/**
 * @brief  简单的哔声（1000Hz，100ms）
 * @note   适用于按键反馈、操作确认等场景
 * @note   ?? 仅能在任务上下文调用
 */
void Buzzer_Beep(void);

/**
 * @brief  开机音效（上升音阶）
 * @note   播放序列：Do-Mi-Sol，音调递增
 * @note   适用于系统启动提示
 * @note   ?? 仅能在任务上下文调用
 */
void Buzzer_StartupSound(void);

/**
 * @brief  成功音效（上升后稳定）
 * @note   播放序列：Sol-高Do-高Mi，愉悦音调
 * @note   适用于操作成功、任务完成等场景
 * @note   ?? 仅能在任务上下文调用
 */
void Buzzer_SuccessSound(void);

/**
 * @brief  错误音效（下降音阶）
 * @note   播放序列：Sol-Mi-Do，音调递减
 * @note   适用于操作失败、错误提示等场景
 * @note   ?? 仅能在任务上下文调用
 */
void Buzzer_ErrorSound(void);

/**
 * @brief  警告音效（交替音）
 * @note   播放 800Hz 和 1200Hz 交替音，重复 3 次
 * @note   适用于报警、严重错误等场景
 * @note   ?? 仅能在任务上下文调用
 */
void Buzzer_WarningSound(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // __BUZZER_H
