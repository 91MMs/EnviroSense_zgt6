/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
// lcd and lvgl
#include <stdio.h>
#include "lcd.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "ui_manager.h"

// sensor and task
#include "sensor_app.h"
#include "devices_manager.h"

#include "adc.h"

// others
#define LOG_MODULE "FREERTOS"
#include "log.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
uint16_t adc_dma_buffer[2] = {0};
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osSemaphoreId sysInitSemaphoreHandle;
/* USER CODE END Variables */
osThreadId defaultTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void SystemAppInitTask(void const* argument);
void SystemMonitorTask(void const* argument);
void VerificationTask_Entry(void const * argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  osSemaphoreDef(sysInitSem); // 定义信号量
  sysInitSemaphoreHandle = osSemaphoreCreate(osSemaphore(sysInitSem), 1); // 创建二进制信号量
  
  // 创建后立即等待一次，使其初始状态为“不可用”或“已被拿走”
  osSemaphoreWait(sysInitSemaphoreHandle, 0); 
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 1024);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  osThreadDef(SystemAppInitTask, SystemAppInitTask, osPriorityNormal, 0, 512);
  osThreadCreate(osThread(SystemAppInitTask), NULL);

  osThreadDef(SystemMonitorTask, SystemMonitorTask, osPriorityIdle, 0, 128);
  osThreadCreate(osThread(SystemMonitorTask), NULL);

  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
    /* Infinite loop */

    // 开启日志
    log_init();
    log_set_level(LOG_LEVEL_INFO);  // 设置日志级别

    lv_init();                // 初始化lvgl
    lv_port_disp_init();      // 初始化显示驱动
    lv_port_indev_init();     // 初始化输入设备驱动

    ui_init();                // 初始化UI管理器

    // [MODIFIED] 所有初始化已完成，释放信号量，通知其他任务可以继续
    osSemaphoreRelease(sysInitSemaphoreHandle);

    for(;;)
    {
        lv_task_handler();
        osDelay(4);
    }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void SystemAppInitTask(void const* argument) {
    // 等待信号量被释放
    osSemaphoreWait(sysInitSemaphoreHandle, osWaitForever); 

    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buffer, 2);

    // 初始化传感器系统
    Sensor_System_Init();

    // 初始化设备管理器
    Drivers_Manager_Init();

    LOG_INFO("系统初始化任务完成，删除本任务");
    osThreadTerminate(osThreadGetId());
}

// 监控任务的函数,LED0闪烁表示系统正在运行
void SystemMonitorTask(void const* argument)
{
    // UBaseType_t hwm_words; // 用于存储返回值（单位：字）
    uint8_t n = 0;
    
    for(;;)
    {
        // 每5秒检查一次
        if (n == 20) {
            n = 0;
            // // 检查 LVGL 任务的堆栈
            // hwm_words = uxTaskGetStackHighWaterMark(defaultTaskHandle);
            // // 将“字”转换为“字节”并打印
            // LOG_INFO("LVGL Task Stack HWM: %u bytes remaining", hwm_words * 4);

            // // 检查传感器任务的堆栈
            // hwm_words = uxTaskGetStackHighWaterMark(SensorTask_GetHandle());
            // LOG_INFO("Sensor Task Stack HWM: %u bytes remaining", hwm_words * 4);

            // // 检查监控任务自身的堆栈
            // hwm_words = uxTaskGetStackHighWaterMark(NULL); // 传入NULL检查自身
            // LOG_INFO("Monitor Task Stack HWM: %u bytes remaining", hwm_words * 4);
            
            // LOG_INFO("---------------------------------");
        }
        HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
        n++;
        osDelay(500);
    }
}
/* USER CODE END Application */
