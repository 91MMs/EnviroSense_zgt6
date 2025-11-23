/**
 ******************************************************************************
 * @file    sensor_task.c
 * @brief   传感器任务管理系统源文件
 * @details 实现了传感器任务的初始化、调度、数据轮询、事件分发及历史数据管理。
 * @author  MmsY
 * @time    2025/11/23
 ******************************************************************************
 */

#include "sensor_task.h"
#include <stdio.h>
#include <string.h>


/* --------------------------- 调试配置 --------------------------- */
#define LOG_MODULE "SensorTask"
#include "log.h"

/* --------------------------- 私有变量 --------------------------- */
static SensorManager_t g_sensor_manager = {0};        // 全局传感器管理器
static SensorEventCallback_t g_event_callback = NULL; // 事件回调函数
static osThreadId sensor_task_handle = NULL;          // 任务句柄

/* --------------------------- 私有函数声明 --------------------------- */
static void SensorTask_MainLoop(void const *argument);
static bool SensorTask_InitializeSensor(SensorInstance_t *sensor);
static bool SensorTask_UpdateSensor(SensorInstance_t *sensor);
static void SensorTask_HandleSensorError(SensorInstance_t *sensor);
static void SensorTask_NotifyEvent(SensorEventType_t event_type,
                                   SensorType_t sensor_type,
                                   const SensorData_t *data,
                                   SensorStatus_t status);

/* --------------------------- 公共函数实现 --------------------------- */

/**
 * @brief 初始化传感器任务管理系统
 */
bool SensorTask_Init(void) {
  if (g_sensor_manager.is_initialized)
    return true;

  LOG_INFO("初始化传感器任务管理系统...");
  memset(&g_sensor_manager, 0, sizeof(SensorManager_t));

  // [新增] 创建全局互斥锁
  osMutexDef(GlobalSensorMutex);
  g_sensor_manager.global_data_mutex =
      osMutexCreate(osMutex(GlobalSensorMutex));

  if (g_sensor_manager.global_data_mutex == NULL) {
    LOG_ERROR("全局互斥锁创建失败");
    return false;
  }

  // 初始化所有传感器实例
  for (int i = 0; i < SENSOR_TYPE_MAX; i++) {
    g_sensor_manager.sensors[i].type = (SensorType_t)i;
    g_sensor_manager.sensors[i].status = SENSOR_STATUS_OFFLINE;
    snprintf(g_sensor_manager.sensors[i].name, SENSOR_MAX_NAME_LEN, "Sensor_%d",
             i);
  }

  // 创建传感器任务
  osThreadDef(sensorTask, SensorTask_MainLoop, SENSOR_TASK_PRIORITY, 0,
              SENSOR_TASK_STACK_SIZE);
  sensor_task_handle = osThreadCreate(osThread(sensorTask), NULL);

  if (sensor_task_handle == NULL) {
    LOG_ERROR("传感器任务创建失败");
    return false;
  }

  g_sensor_manager.is_initialized = true;
  g_sensor_manager.active_sensor_count = 0;
  LOG_INFO("传感器任务创建成功");

  return true;
}

/**
 * @brief 注册传感器
 */
bool SensorTask_RegisterSensor(SensorType_t type, const char *name,
                               const SensorCallbacks_t *callbacks,
                               void *device_handle,
                               uint32_t update_interval_ms) {
  if (!g_sensor_manager.is_initialized || type >= SENSOR_TYPE_MAX ||
      type == SENSOR_TYPE_NONE || callbacks == NULL ||
      callbacks->init_func == NULL || callbacks->read_func == NULL) {
    LOG_ERROR("注册传感器失败：参数无效 (type: %d)", type);
    return false;
  }

  SensorInstance_t *sensor = &g_sensor_manager.sensors[type];
  strncpy(sensor->name, name, SENSOR_MAX_NAME_LEN - 1);
  sensor->name[SENSOR_MAX_NAME_LEN - 1] = '\0';
  sensor->device_handle = device_handle;
  sensor->update_interval_ms = update_interval_ms;
  sensor->error_count = 0;
  sensor->is_enabled = false;

  // 复制回调函数
  g_sensor_manager.callbacks[type] = *callbacks;

  // 启用传感器
  SensorTask_EnableSensor(type);

  LOG_INFO("传感器 '%s' (类型: %d) 注册完成", sensor->name, type);
  return true;
}

/**
 * @brief 启用传感器
 */
bool SensorTask_EnableSensor(SensorType_t type) {
  if (!g_sensor_manager.is_initialized || type >= SENSOR_TYPE_MAX ||
      type == SENSOR_TYPE_NONE) {
    return false;
  }

  SensorInstance_t *sensor = &g_sensor_manager.sensors[type];

  if (!sensor->is_enabled) {
    sensor->is_enabled = true;
    sensor->status = SENSOR_STATUS_INITIALIZING;
    sensor->error_count = 0;
    g_sensor_manager.active_sensor_count++;

    LOG_INFO("启用传感器: %s", sensor->name);

    // 通知状态变化事件
    SensorTask_NotifyEvent(SENSOR_EVENT_STATUS_CHANGE, type, NULL,
                           SENSOR_STATUS_INITIALIZING);
  }
  return true;
}

/**
 * @brief 禁用传感器
 */
bool SensorTask_DisableSensor(SensorType_t type) {
  if (!g_sensor_manager.is_initialized || type >= SENSOR_TYPE_MAX ||
      type == SENSOR_TYPE_NONE) {
    return false;
  }

  SensorInstance_t *sensor = &g_sensor_manager.sensors[type];

  if (sensor->is_enabled) {
    // 调用反初始化函数
    if (g_sensor_manager.callbacks[type].deinit_func != NULL) {
      g_sensor_manager.callbacks[type].deinit_func(sensor);
    }

    sensor->is_enabled = false;
    sensor->status = SENSOR_STATUS_OFFLINE;
    g_sensor_manager.active_sensor_count--;

    LOG_INFO("禁用传感器: %s", sensor->name);

    // 通知状态变化事件
    SensorTask_NotifyEvent(SENSOR_EVENT_STATUS_CHANGE, type, NULL,
                           SENSOR_STATUS_OFFLINE);
  }

  return true;
}

/**
 * @brief 获取传感器数据
 */
bool SensorTask_GetSensorData(SensorType_t type, SensorData_t *data) {
  if (!g_sensor_manager.is_initialized || type >= SENSOR_TYPE_MAX ||
      type == SENSOR_TYPE_NONE || data == NULL) {
    return false;
  }

  SensorInstance_t *sensor = &g_sensor_manager.sensors[type];

  if (sensor->is_enabled && sensor->data.is_valid) {
    if (osMutexWait(g_sensor_manager.global_data_mutex, 100) == osOK) {
      *data = sensor->data;
      osMutexRelease(g_sensor_manager.global_data_mutex);
      return true;
    }
  }

  return false;
}

/**
 * @brief [新增] 获取指定传感器的状态
 * @param type 传感器类型
 * @param status 输出状态的指针
 * @return true: 成功, false: 失败
 */
bool SensorTask_GetSensorStatus(SensorType_t type, SensorStatus_t *status) {
  if (!g_sensor_manager.is_initialized || type >= SENSOR_TYPE_MAX ||
      status == NULL) {
    return false;
  }

  SensorInstance_t *sensor = &g_sensor_manager.sensors[type];

  if (osMutexWait(g_sensor_manager.global_data_mutex, 100) == osOK) {
    *status = sensor->status;
    osMutexRelease(g_sensor_manager.global_data_mutex);
    return true;
  }

  return false;
}

/**
 * @brief 设置传感器更新间隔
 */
bool SensorTask_SetUpdateInterval(SensorType_t type, uint32_t interval_ms) {
  if (!g_sensor_manager.is_initialized || type >= SENSOR_TYPE_MAX ||
      type == SENSOR_TYPE_NONE || interval_ms < 100) {
    return false;
  }

  g_sensor_manager.sensors[type].update_interval_ms = interval_ms;
  LOG_INFO("设置传感器 %s 更新间隔为 %d ms",
           g_sensor_manager.sensors[type].name, interval_ms);

  return true;
}

/**
 * @brief 注册传感器事件回调函数
 */
bool SensorTask_RegisterEventCallback(SensorEventCallback_t callback) {
  g_event_callback = callback;
  return true;
}

/* --------------------------- 私有函数实现 --------------------------- */

/**
 * @brief 传感器任务主循环
 */
static void SensorTask_MainLoop(void const *argument) {
  osDelay(1000); // 等待系统稳定

  uint8_t loop_count = 0;

  for (;;) {
    uint32_t loop_start_time = HAL_GetTick();

    // 遍历所有传感器
    for (int i = 1; i < SENSOR_TYPE_MAX; i++) { // 跳过SENSOR_TYPE_NONE
      SensorInstance_t *sensor = &g_sensor_manager.sensors[i];

      if (!sensor->is_enabled) {
        continue; // 跳过未启用的传感器
      }

      // 检查是否需要初始化
      if (sensor->status == SENSOR_STATUS_INITIALIZING) {
        if (SensorTask_InitializeSensor(sensor)) {
          sensor->status = SENSOR_STATUS_ONLINE;
          sensor->last_update_time = HAL_GetTick();
          LOG_INFO("传感器 %s 初始化成功", sensor->name);

          // 通知状态变化事件
          SensorTask_NotifyEvent(SENSOR_EVENT_STATUS_CHANGE, (SensorType_t)i,
                                 NULL, SENSOR_STATUS_ONLINE);
        } else {
          SensorTask_HandleSensorError(sensor);
          continue;
        }
      }

      // 检查是否需要更新数据
      uint32_t current_time = HAL_GetTick();
      if ((current_time - sensor->last_update_time) >=
          sensor->update_interval_ms) {
        if (SensorTask_UpdateSensor(sensor)) {
          sensor->last_update_time = current_time;

          // 通知数据更新事件
          SensorTask_NotifyEvent(SENSOR_EVENT_DATA_UPDATE, (SensorType_t)i,
                                 &sensor->data, sensor->status);
        } else {
          SensorTask_HandleSensorError(sensor);
        }
      }
    }

    // 每100个循环打印一次状态（约10秒）一个循环大概100ms
    if (++loop_count % 100 == 0) {
      loop_count = 0;
      LOG_INFO("传感器任务运行正常，系统运行时间:%d ms 活跃传感器: %d",
               HAL_GetTick(), g_sensor_manager.active_sensor_count);
    }

    // 动态调整循环间隔
    uint32_t loop_duration = HAL_GetTick() - loop_start_time;
    uint32_t sleep_time = (loop_duration < 100) ? (100 - loop_duration) : 10;

    osDelay(sleep_time);
  }
}

/**
 * @brief 初始化传感器
 */
static bool SensorTask_InitializeSensor(SensorInstance_t *sensor) {
  if (sensor == NULL || sensor->type >= SENSOR_TYPE_MAX) {
    return false;
  }

  SensorCallbacks_t *callbacks = &g_sensor_manager.callbacks[sensor->type];

  if (callbacks->init_func != NULL) {
    return callbacks->init_func(sensor);
  }

  return false;
}

/**
 * @brief 更新传感器数据，并记录历史和统计信息
 */
static bool SensorTask_UpdateSensor(SensorInstance_t *sensor) {
  if (sensor == NULL || sensor->type >= SENSOR_TYPE_MAX) {
    return false;
  }

  SensorCallbacks_t *callbacks = &g_sensor_manager.callbacks[sensor->type];

  if (callbacks->read_func != NULL) {
    // 调用底层驱动的读取函数
    bool result = callbacks->read_func(sensor);

    if (result) {
      if (osMutexWait(g_sensor_manager.global_data_mutex, osWaitForever) ==
          osOK) {
        sensor->data.timestamp = HAL_GetTick();
        sensor->data.is_valid = true;
        sensor->error_count = 0;

        // [NEW] --- 开始更新历史和统计数据 ---

        // 1. 提取当前读数 (统一为 float 类型处理)
        float primary_value = 0.0f;
        float secondary_value = 0.0f;
        switch (sensor->type) {
        case SENSOR_TYPE_SHT30:
          primary_value = sensor->data.values.sht30.temp;
          secondary_value = sensor->data.values.sht30.humi;
          break;
        case SENSOR_TYPE_GY30:
          primary_value = sensor->data.values.gy30.lux;
          break;
        case SENSOR_TYPE_SMOKE:
          primary_value = (float)sensor->data.values.smoke.ppm;
          break;
        default:
          break;
        }

        // 2. 更新历史数据 (循环缓冲区)
        sensor->history[sensor->history_head] = primary_value;
        if (sensor->type == SENSOR_TYPE_SHT30) {
          sensor->secondary_history[sensor->history_head] = secondary_value;
        }
        sensor->history_head = (sensor->history_head + 1) % SENSOR_HISTORY_SIZE;
        if (sensor->history_count < SENSOR_HISTORY_SIZE) {
          sensor->history_count++;
        }

        // 3. 更新统计数据
        if (sensor->history_count == 1) { // 如果是第一个数据点
          sensor->stats.min = primary_value;
          sensor->stats.max = primary_value;
          sensor->stats.avg = primary_value;
          if (sensor->type == SENSOR_TYPE_SHT30) { // SHT30有第二组数据
            sensor->secondary_stats.min = secondary_value;
            sensor->secondary_stats.max = secondary_value;
            sensor->secondary_stats.avg = secondary_value;
          }
        } else {
          // 更新最大/最小值
          if (primary_value < sensor->stats.min)
            sensor->stats.min = primary_value;
          if (primary_value > sensor->stats.max)
            sensor->stats.max = primary_value;
          if (sensor->type == SENSOR_TYPE_SHT30) {
            if (secondary_value < sensor->secondary_stats.min)
              sensor->secondary_stats.min = secondary_value;
            if (secondary_value > sensor->secondary_stats.max)
              sensor->secondary_stats.max = secondary_value;
          }

          // 重新计算平均值 (对于少量数据，直接重算最准确)
          int start_idx = (sensor->history_head - sensor->history_count +
                           SENSOR_HISTORY_SIZE) %
                          SENSOR_HISTORY_SIZE;
          float p_sum = 0.0f, p_min = sensor->history[start_idx],
                p_max = sensor->history[start_idx];
          float s_sum = 0.0f, s_min = 0.0f, s_max = 0.0f;
          if (sensor->type == SENSOR_TYPE_SHT30) {
            s_min = sensor->secondary_history[start_idx];
            s_max = sensor->secondary_history[start_idx];
          }

          for (int i = 0; i < sensor->history_count; i++) {
            int read_idx = (start_idx + i) % SENSOR_HISTORY_SIZE;
            float p_val = sensor->history[read_idx];
            p_sum += p_val;
            if (p_val < p_min)
              p_min = p_val;
            if (p_val > p_max)
              p_max = p_val;

            if (sensor->type == SENSOR_TYPE_SHT30) {
              float s_val = sensor->secondary_history[read_idx];
              s_sum += s_val;
              if (s_val < s_min)
                s_min = s_val;
              if (s_val > s_max)
                s_max = s_val;
            }
          }

          // 更新所有局部统计值和平均值
          sensor->stats.local_min = p_min;
          sensor->stats.local_max = p_max;
          sensor->stats.local_avg = p_sum / sensor->history_count;
          sensor->stats.avg =
              sensor->stats.local_avg; // 全局平均值也更新为局部平均值

          if (sensor->type == SENSOR_TYPE_SHT30) {
            sensor->secondary_stats.local_min = s_min;
            sensor->secondary_stats.local_max = s_max;
            sensor->secondary_stats.local_avg = s_sum / sensor->history_count;
            sensor->secondary_stats.avg = sensor->secondary_stats.local_avg;
          }
        }
        osMutexRelease(g_sensor_manager.global_data_mutex);
      } // end if(osMutexWait

    } // end if(result)

    return result;
  }

  return false;
}

/**
 * @brief 获取指定传感器的统计数据
 * @param type 传感器类型
 * @param stats 输出统计数据的指针
 * @return true: 成功, false: 失败或无数据
 */
bool SensorTask_GetStats(SensorType_t type, SensorStats_t *stats) {
  if (!g_sensor_manager.is_initialized || type >= SENSOR_TYPE_MAX ||
      stats == NULL) {
    return false;
  }

  SensorInstance_t *sensor = &g_sensor_manager.sensors[type];

  // 确保传感器已启用且至少有一个数据点
  if (sensor->is_enabled && sensor->history_count > 0) {
    // TODO: 考虑线程安全，可以加锁
    if (osMutexWait(g_sensor_manager.global_data_mutex, 100) == osOK) {
      *stats = sensor->stats;
      osMutexRelease(g_sensor_manager.global_data_mutex);
      return true;
    }
  }

  return false;
}

/**
 * @brief 获取指定传感器的次要统计数据 (如湿度)
 * @param type 传感器类型
 * @param stats 输出统计数据的指针
 * @return true: 成功, false: 失败或无数据
 */
bool SensorTask_GetSecondaryStats(SensorType_t type, SensorStats_t *stats) {
  if (!g_sensor_manager.is_initialized || type >= SENSOR_TYPE_MAX ||
      stats == NULL) {
    return false;
  }

  // 只有SHT30有次要统计数据
  if (type != SENSOR_TYPE_SHT30) {
    return false;
  }

  SensorInstance_t *sensor = &g_sensor_manager.sensors[type];

  // 确保传感器已启用且至少有一个数据点
  if (sensor->is_enabled && sensor->history_count > 0) {
    // 加锁读取次要统计数据
    if (osMutexWait(g_sensor_manager.global_data_mutex, 100) == osOK) {
      *stats = sensor->secondary_stats;
      osMutexRelease(g_sensor_manager.global_data_mutex);
      return true;
    }
  }

  return false;
}

/**
 * @brief 获取指定传感器的主要历史数据 (已按时间排好序)
 * @param type 传感器类型
 * @param history_data 输出一个指向有序历史数据的指针的指针
 * @return uint16_t 有效的历史数据点数量
 */
uint16_t SensorTask_GetPrimaryHistory(SensorType_t type,
                                      const float **history_data) {
  // 使用静态数组作为临时缓冲区，避免在栈上分配大数组
  static float ordered_primary_history[SENSOR_HISTORY_SIZE];

  if (!g_sensor_manager.is_initialized || type >= SENSOR_TYPE_MAX ||
      history_data == NULL) {
    *history_data = NULL;
    return 0;
  }

  SensorInstance_t *sensor = &g_sensor_manager.sensors[type];

  if (sensor->is_enabled && sensor->history_count > 0) {
    // 加锁读取历史数据
    if (osMutexWait(g_sensor_manager.global_data_mutex, 100) == osOK) {
      // --- 核心逻辑：将循环缓冲区的数据“展开”为按时间排序的线性数组 ---
      // 缓冲区未满：从索引0开始读取 / 缓冲区已满：从 history_head
      // 的下一个位置开始（最旧的数据）
      int read_idx = (sensor->history_count < SENSOR_HISTORY_SIZE)
                         ? 0
                         : sensor->history_head;

      // 按时间顺序（从旧到新）复制数据
      for (int i = 0; i < sensor->history_count; i++) {
        ordered_primary_history[i] = sensor->history[read_idx];
        read_idx = (read_idx + 1) % SENSOR_HISTORY_SIZE;
      }
      osMutexRelease(g_sensor_manager.global_data_mutex);
      *history_data = ordered_primary_history;
      return sensor->history_count;
    }
  }

  *history_data = NULL;
  return 0;
}
/**
 * @brief 获取指定传感器的第二组历史数据 (已按时间排好序)
 * @details 主要用于获取SHT30的湿度历史数据。
 * @param type 传感器类型
 * @param history_data 输出一个指向有序历史数据的指针的指针
 * @return uint16_t 有效的历史数据点数量
 */
uint16_t SensorTask_GetSecondaryHistory(SensorType_t type,
                                        const float **history_data) {
  static float ordered_secondary_history[SENSOR_HISTORY_SIZE];

  if (!g_sensor_manager.is_initialized || type >= SENSOR_TYPE_MAX ||
      history_data == NULL) {
    *history_data = NULL;
    return 0;
  }

  // 只有SHT30传感器有第二组历史数据
  if (type != SENSOR_TYPE_SHT30) {
    *history_data = NULL;
    return 0;
  }

  SensorInstance_t *sensor = &g_sensor_manager.sensors[type];

  if (sensor->is_enabled && sensor->history_count > 0) {
    // 加锁读取次要历史数据
    if (osMutexWait(g_sensor_manager.global_data_mutex, 100) == osOK) {
      // 缓冲区未满：从索引0开始读取 / 缓冲区已满：从 history_head
      // 的下一个位置开始（最旧的数据）
      int read_idx = (sensor->history_count < SENSOR_HISTORY_SIZE)
                         ? 0
                         : sensor->history_head;

      // 按时间顺序（从旧到新）复制数据
      for (int i = 0; i < sensor->history_count; i++) {
        ordered_secondary_history[i] = sensor->secondary_history[read_idx];
        read_idx = (read_idx + 1) % SENSOR_HISTORY_SIZE;
      }

      osMutexRelease(g_sensor_manager.global_data_mutex);
      *history_data = ordered_secondary_history;
      return sensor->history_count;
    }
  }

  *history_data = NULL;
  return 0;
}

/**
 * @brief 处理传感器错误
 */
static void SensorTask_HandleSensorError(SensorInstance_t *sensor) {
  sensor->error_count++;

  LOG_WARN("传感器 %s 错误，错误次数: %lu", sensor->name, sensor->error_count);

  // 错误次数到达5次，将传感器标记为初始化中等待重新初始化
  if (sensor->error_count == 5) {
    LOG_WARN("尝试重新初始化传感器 %s", sensor->name);
    sensor->status = SENSOR_STATUS_INITIALIZING;
  }
  // 错误次数到达10次，将传感器标记为错误状态,并禁用传感器
  if (sensor->error_count == 10) {
    SensorTask_DisableSensor(sensor->type);
    sensor->status = SENSOR_STATUS_ERROR;
    SensorTask_NotifyEvent(SENSOR_EVENT_STATUS_CHANGE, sensor->type, NULL,
                           SENSOR_STATUS_ERROR);
  }
}

/**
 * @brief 通知传感器事件
 */
static void SensorTask_NotifyEvent(SensorEventType_t event_type,
                                   SensorType_t sensor_type,
                                   const SensorData_t *data,
                                   SensorStatus_t status) {
  if (g_event_callback != NULL) {
    SensorEvent_t event = {
        .event_type = event_type, .sensor_type = sensor_type, .status = status};

    if (data != NULL) {
      event.data = *data;
    }

    g_event_callback(&event);
  }
}

/**
 * @brief 获取传感器状态枚举字符串
 */
const char *SensorStatus_ToString(SensorStatus_t status) {
  switch (status) {
  case SENSOR_STATUS_OFFLINE:
    return "OFFLINE";
  case SENSOR_STATUS_ONLINE:
    return "ONLINE";
  case SENSOR_STATUS_ERROR:
    return "ERROR";
  case SENSOR_STATUS_INITIALIZING:
    return "INITIALIZING";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief 获取传感器类型枚举字符串
 */
const char *SensorType_ToString(SensorType_t type) {
  switch (type) {
  case SENSOR_TYPE_NONE:
    return "NONE";
  case SENSOR_TYPE_GY30:
    return "GY30";
  case SENSOR_TYPE_SHT30:
    return "SHT30";
  case SENSOR_TYPE_SMOKE:
    return "MQ-2";
  case SENSOR_TYPE_MAX:
    return "MAX";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief 获取传感器事件类型枚举字符串
 */
const char *SensorEventType_ToString(SensorEventType_t event) {
  switch (event) {
  case SENSOR_EVENT_DATA_UPDATE:
    return "DATA_UPDATE";
  case SENSOR_EVENT_STATUS_CHANGE:
    return "STATUS_CHANGE";
  case SENSOR_EVENT_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN_EVENT";
  }
}

/**
 * @brief 返回传感器任务的 FreeRTOS 句柄（只读）
 * @return osThreadId 传感器任务句柄，若未创建返回 NULL
 */
osThreadId SensorTask_GetHandle(void) { return sensor_task_handle; }
