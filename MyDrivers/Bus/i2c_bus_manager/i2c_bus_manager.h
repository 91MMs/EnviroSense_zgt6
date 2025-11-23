#ifndef __I2C_BUS_MANAGER_H
#define __I2C_BUS_MANAGER_H

#include "main.h"
#include "cmsis_os.h" // 推荐：如果用到RTOS
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化I2C总线管理器
 * @note  此函数会创建用于保护I2C总线的互斥信号量。
 *        应在RTOS内核启动前调用。
 * @param None
 * @return true  初始化成功
 * @return false 初始化失败
 */
bool I2C_Bus_Manager_Init(void);

/**
 * @brief 申请I2C总线
 * @note  任何任务在开始一系列I2C操作前应调用此函数。
 *        若长时间未获得总线会超时返回。
 * @param timeout_ms 等待超时时间 (毫秒)
 * @return true 成功获得锁
 * @return false 获得锁超时
 */
bool I2C_Bus_Lock(uint32_t timeout_ms);

/**
 * @brief 释放I2C总线
 * @note  完成所有I2C操作后应调用此函数释放总线
 * @param None
 * @return None
 */
void I2C_Bus_Unlock(void);

#ifdef  __cplusplus
}
#endif

#endif //
