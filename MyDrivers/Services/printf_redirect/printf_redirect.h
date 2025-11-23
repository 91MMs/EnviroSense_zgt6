/* printf_redirect.h */
#ifndef PRINTF_REDIRECT_H__
#define PRINTF_REDIRECT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdio.h>
#include <stdint.h>

/* 配置参数 - 可根据项目需要修改 */
#define PRINTF_BUFFER_SIZE          512
#define PRINTF_MUTEX_TIMEOUT_MS     100
#define PRINTF_USE_FREERTOS         1       // 是否使用FreeRTOS
#define PRINTF_USE_DMA              1       // 是否使用DMA

/* 如果使用FreeRTOS */
#if PRINTF_USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#endif

/* 基本接口 */
void printf_init(UART_HandleTypeDef* huart);
void printf_flush(void);
uint8_t printf_is_busy(void);
void printf_get_status(uint16_t* pending_chars, uint8_t* is_transmitting);
void printf_write_string(const char* str);
int fputc_nb(int ch, FILE *f);

/* 辅助函数 - 可动态设置UART句柄 */
void printf_set_uart_handle(UART_HandleTypeDef* huart);

/* 回调函数 */
void printf_uart_tx_complete_callback(UART_HandleTypeDef *huart);
void printf_uart_error_callback(UART_HandleTypeDef *huart);

/* 调试接口 */
#if PRINTF_USE_FREERTOS
void printf_debug_mutex_status(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __PRINTF_REDIRECT_H__ */
