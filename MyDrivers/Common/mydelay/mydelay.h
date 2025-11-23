/**
 * @file mydelay.c
 * @brief 使用hal库的的延时函数来代替标准库的延时函数
 * @author MmsY
 * @date 2025
*/
 
#ifndef __MYDELAY_H
#define __MYDELAY_H

#include "main.h"

void delay_us(uint32_t nus);
void delay_ms(uint32_t nms);

#endif

