/**
 * @file mydelay.c
 * @brief 使用hal库的的延时函数来代替标准库的延时函数
 * @author MmsY
 * @date 2025
*/

#include "mydelay.h"

// 自己模拟实现的
void delay_us(uint32_t nus) {
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;
    
    ticks = nus * (SystemCoreClock / 1000000);  // 计算需要的节拍数
    told = SysTick->VAL;                        // 刚进入时的计数器值
    
    while (1)
    {
        tnow = SysTick->VAL;
        if (tnow != told)
        {
            // SYSTICK是一个递减的计数器
            if (tnow < told) {
                tcnt += told - tnow;
            }
            else {
                // 重新装载
                tcnt += reload - tnow + told;
            }
            told = tnow;
            
            // 时间超过/等于要延迟的时间,则退出
            if (tcnt >= ticks) {
                break;
            }
        }
    }
}

// 使用HAL_Delay代替delay_ms
void delay_ms(uint32_t nms) {
    HAL_Delay(nms);
}
