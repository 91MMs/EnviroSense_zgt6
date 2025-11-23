/* printf_redirect.c */
#include "printf_redirect.h"
#include <string.h>

/* 双缓冲区 */
static char printf_buffer_a[PRINTF_BUFFER_SIZE];
static char printf_buffer_b[PRINTF_BUFFER_SIZE];
static volatile char* current_write_buffer;
static volatile char* current_dma_buffer;
static volatile uint16_t write_index = 0;
static volatile uint16_t dma_length = 0;
static volatile uint8_t dma_busy = 0;
static volatile uint8_t buffer_switch_pending = 0;

/* UART句柄指针 - 通过外部函数设置 */
static UART_HandleTypeDef* printf_uart_handle = NULL;

#if PRINTF_USE_FREERTOS
/* FreeRTOS相关变量 */
static SemaphoreHandle_t printf_mutex = NULL;
static StaticSemaphore_t printf_mutex_buffer;
#define PRINTF_MUTEX_TIMEOUT pdMS_TO_TICKS(PRINTF_MUTEX_TIMEOUT_MS)

/* 判断是否在中断上下文 */
static inline uint32_t is_in_isr(void)
{
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;
}

/* 安全的获取互斥锁 */
static BaseType_t take_mutex_safe(TickType_t timeout)
{
    if (is_in_isr() || printf_mutex == NULL) {
        return pdFAIL;
    }
    return xSemaphoreTake(printf_mutex, timeout);
}

static void give_mutex_safe(void)
{
    if (!is_in_isr() && printf_mutex != NULL) {
        xSemaphoreGive(printf_mutex);
    }
}

#else
/* 非RTOS环境下的临界区保护实现 */
static void take_mutex_safe(uint32_t timeout)
{
    __disable_irq();
}

static void give_mutex_safe(void)
{
    __enable_irq();
}
#endif

/* 设置UART句柄 */
void printf_set_uart_handle(UART_HandleTypeDef* huart)
{
    printf_uart_handle = huart;
}

/* 切换缓冲区 */
static void switch_buffer(void)
{
    if (current_write_buffer == printf_buffer_a) {
        current_write_buffer = printf_buffer_b;
        current_dma_buffer = printf_buffer_a;
    } else {
        current_write_buffer = printf_buffer_a;
        current_dma_buffer = printf_buffer_b;
    }
    
    dma_length = write_index;
    write_index = 0;
    buffer_switch_pending = 0;
}

/* 启动DMA传输 */
static void start_dma_transmission(void)
{
    if (printf_uart_handle == NULL) {
        return;
    }
    
    if (!dma_busy && dma_length > 0) {
        dma_busy = 1;
        
#if PRINTF_USE_DMA
        if (HAL_UART_Transmit_DMA(printf_uart_handle, (uint8_t*)current_dma_buffer, dma_length) != HAL_OK) {
            /* DMA发送失败，自动切换到中断模式 */
            dma_busy = 0;
            HAL_UART_Transmit_IT(printf_uart_handle, (uint8_t*)current_dma_buffer, dma_length);
            dma_busy = 1;
        }
#else
        /* 使用中断模式 */
        HAL_UART_Transmit_IT(printf_uart_handle, (uint8_t*)current_dma_buffer, dma_length);
#endif
    }
}

/* HAL回调函数 - 需要在main.c的用户代码中调用 */
void printf_uart_tx_complete_callback(UART_HandleTypeDef *huart)
{
    if (huart == printf_uart_handle) {
        dma_busy = 0;
        dma_length = 0;
        
        /* 如果有缓冲区切换请求，则切换并继续发送 */
        if (buffer_switch_pending) {
            switch_buffer();
            start_dma_transmission();
        }
    }
}

void printf_uart_error_callback(UART_HandleTypeDef *huart)
{
    if (huart == printf_uart_handle) {
        dma_busy = 0;
        
        /* 发生错误时尝试重新发送 */
        if (dma_length > 0) {
#if PRINTF_USE_DMA
            if (HAL_UART_Transmit_DMA(printf_uart_handle, (uint8_t*)current_dma_buffer, dma_length) == HAL_OK) {
                dma_busy = 1;
            }
#else
            if (HAL_UART_Transmit_IT(printf_uart_handle, (uint8_t*)current_dma_buffer, dma_length) == HAL_OK) {
                dma_busy = 1;
            }
#endif
        }
    }
}

/* 内部字符写入函数 */
static int internal_putchar(int ch, uint8_t blocking)
{
    if (printf_uart_handle == NULL) {
        return -1;
    }
    
    /* 检查当前缓冲区是否有空间 */
    if (write_index >= PRINTF_BUFFER_SIZE - 1) {
        /* 当前缓冲区满，需要切换 */
        if (dma_busy) {
            if (!blocking) {
                return -1; // 非阻塞模式下返回失败
            }
            
            /* 阻塞模式下等待切换完成 */
            buffer_switch_pending = 1;
            
#if PRINTF_USE_FREERTOS
            /* 临时释放互斥锁，避免死锁 */
            give_mutex_safe();
            
            /* 等待DMA完成并切换缓冲区 */
            while (buffer_switch_pending) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            
            /* 重新获取互斥锁 */
            if (take_mutex_safe(PRINTF_MUTEX_TIMEOUT) != pdTRUE) {
                return -1;
            }
#else
            /* 非RTOS环境下简单等待 */
            give_mutex_safe();
            while (buffer_switch_pending) {
                HAL_Delay(1);
            }
            take_mutex_safe(0);
#endif
        } else {
            /* DMA空闲，直接切换缓冲区 */
            switch_buffer();
        }
    }
    
    /* 写入字符到当前写缓冲区 */
    ((char*)current_write_buffer)[write_index++] = (char)ch;
    
    /* 如果是换行符或缓冲区接近满，则尝试发送 */
    if (ch == '\n' || ch == '\r' || write_index >= PRINTF_BUFFER_SIZE - 10) {
        if (!dma_busy && write_index > 0) {
            switch_buffer();
            start_dma_transmission();
        } else if (dma_busy) {
            buffer_switch_pending = 1;
        }
    }
    
    return ch;
}

/* 重写fputc函数 */
int fputc(int ch, FILE *f)
{
#if PRINTF_USE_FREERTOS
    /* 如果在中断中使用简化版本直接写入 */
    if (is_in_isr()) {
        if (write_index < PRINTF_BUFFER_SIZE - 1) {
            ((char*)current_write_buffer)[write_index++] = (char)ch;
            return ch;
        }
        return -1;
    }
    
    /* 获取互斥锁 */
    if (take_mutex_safe(PRINTF_MUTEX_TIMEOUT) != pdTRUE) {
        return -1;
    }
#else
    take_mutex_safe(0);
#endif
    
    int result = internal_putchar(ch, 1); // 阻塞模式
    
    give_mutex_safe();
    
    return result;
}

/* 非阻塞版本fputc */
int fputc_nb(int ch, FILE *f)
{
#if PRINTF_USE_FREERTOS
    if (is_in_isr()) {
        return fputc(ch, f);
    }
    
    if (take_mutex_safe(0) != pdTRUE) {
        return -1;
    }
#else
    take_mutex_safe(0);
#endif
    
    int result = internal_putchar(ch, 0); // 非阻塞模式
    
    give_mutex_safe();
    
    return result;
}

/* 强制刷新缓冲区 */
void printf_flush(void)
{
#if PRINTF_USE_FREERTOS
    if (is_in_isr()) {
        return;
    }
    
    if (take_mutex_safe(PRINTF_MUTEX_TIMEOUT) != pdTRUE) {
        return;
    }
#else
    take_mutex_safe(0);
#endif
    
    if (write_index > 0) {
        if (!dma_busy) {
            switch_buffer();
            start_dma_transmission();
        } else {
            buffer_switch_pending = 1;
        }
    }
    
    give_mutex_safe();
    
    /* 等待所有数据发送完成 */
#if PRINTF_USE_FREERTOS
    while (dma_busy || buffer_switch_pending) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
#else
    while (dma_busy || buffer_switch_pending) {
        HAL_Delay(1);
    }
#endif
}

/* 查询传输状态 */
uint8_t printf_is_busy(void)
{
    uint8_t busy = 0;
    
#if PRINTF_USE_FREERTOS
    if (take_mutex_safe(pdMS_TO_TICKS(10)) == pdTRUE) {
        busy = (dma_busy || buffer_switch_pending || write_index > 0);
        give_mutex_safe();
    }
#else
    take_mutex_safe(0);
    busy = (dma_busy || buffer_switch_pending || write_index > 0);
    give_mutex_safe();
#endif
    
    return busy;
}

/* 获取状态信息 */
void printf_get_status(uint16_t* pending_chars, uint8_t* is_transmitting)
{
#if PRINTF_USE_FREERTOS
    if (take_mutex_safe(pdMS_TO_TICKS(10)) == pdTRUE) {
        *pending_chars = write_index;
        *is_transmitting = dma_busy;
        give_mutex_safe();
    } else {
        *pending_chars = 0;
        *is_transmitting = 0;
    }
#else
    take_mutex_safe(0);
    *pending_chars = write_index;
    *is_transmitting = dma_busy;
    give_mutex_safe();
#endif
}

/* 写字符串接口 */
void printf_write_string(const char* str)
{
    if (str == NULL || printf_uart_handle == NULL) {
        return;
    }
    
#if PRINTF_USE_FREERTOS
    if (is_in_isr()) {
        return;
    }
    
    if (take_mutex_safe(PRINTF_MUTEX_TIMEOUT) != pdTRUE) {
        return;
    }
#else
    take_mutex_safe(0);
#endif
    
    uint16_t len = strlen(str);
    uint16_t i = 0;
    
    while (i < len) {
        uint16_t available = PRINTF_BUFFER_SIZE - 1 - write_index;
        uint16_t to_write = (len - i) < available ? (len - i) : available;
        
        if (to_write == 0) {
            if (dma_busy) {
                buffer_switch_pending = 1;
                give_mutex_safe();
                
#if PRINTF_USE_FREERTOS
                while (buffer_switch_pending) {
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
                
                if (take_mutex_safe(PRINTF_MUTEX_TIMEOUT) != pdTRUE) {
                    return;
                }
#else
                while (buffer_switch_pending) {
                    HAL_Delay(1);
                }
                take_mutex_safe(0);
#endif
                continue;
            } else {
                switch_buffer();
                start_dma_transmission();
                continue;
            }
        }
        
        memcpy((char*)current_write_buffer + write_index, str + i, to_write);
        write_index += to_write;
        i += to_write;
        
        if (write_index >= PRINTF_BUFFER_SIZE - 10) {
            if (!dma_busy) {
                switch_buffer();
                start_dma_transmission();
            } else {
                buffer_switch_pending = 1;
            }
        }
    }
    
    give_mutex_safe();
}

/* 初始化函数 */
void printf_init(UART_HandleTypeDef* huart)
{
    printf_set_uart_handle(huart);
#if PRINTF_USE_FREERTOS
    /* 创建静态互斥锁 */
    printf_mutex = xSemaphoreCreateMutexStatic(&printf_mutex_buffer);
    if (printf_mutex == NULL) {
        HAL_UART_Transmit(huart, (uint8_t*)"Mutex Create Failed\r\n", 21, 1000);
    }
#endif
    
    /* 初始化缓冲区指针和状态 */
    current_write_buffer = printf_buffer_a;
    current_dma_buffer = printf_buffer_b;
    write_index = 0;
    dma_length = 0;
    dma_busy = 0;
    buffer_switch_pending = 0;
    
    /* 清空缓冲区 */
    memset(printf_buffer_a, 0, PRINTF_BUFFER_SIZE);
    memset(printf_buffer_b, 0, PRINTF_BUFFER_SIZE);
}

#if PRINTF_USE_FREERTOS
/* 调试互斥锁状态 */
void printf_debug_mutex_status(void)
{
    if (printf_mutex != NULL) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(printf_mutex);
        if (holder != NULL) {
            printf("Printf mutex held by: %s\r\n", pcTaskGetName(holder));
        } else {
            printf("Printf mutex is free\r\n");
        }
    }
}
#endif
