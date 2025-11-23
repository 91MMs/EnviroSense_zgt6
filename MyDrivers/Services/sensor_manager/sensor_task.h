/**
 * @file sensor_task.h
 * @brief 传感器任务管理系统头文件
 * @author MmsY
 * @date 2025
 */

#ifndef __SENSOR_TASK_H
#define __SENSOR_TASK_H

#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* --------------------------- 系统配置 --------------------------- */
#define SENSOR_TASK_STACK_SIZE      512    // 传感器任务栈大小
#define SENSOR_TASK_PRIORITY        osPriorityBelowNormal
#define SENSOR_UPDATE_INTERVAL_MS   2000    // 默认传感器更新间隔 (2秒)
#define SENSOR_MAX_NAME_LEN         32      // 传感器名称最大长度

/* --------------------------- 传感器类型枚举 --------------------------- */
typedef enum {
    SENSOR_TYPE_NONE = 0,       // 无传感器
    SENSOR_TYPE_GY30,           // GY-30 光照传感器
    SENSOR_TYPE_SHT30,          // SHT30 温湿度传感器
    SENSOR_TYPE_SMOKE,          // 烟雾传感器
    SENSOR_TYPE_MAX             // 传感器类型最大值
} SensorType_t;

/* --------------------------- 传感器状态枚举 --------------------------- */
typedef enum {
    SENSOR_STATUS_OFFLINE = 0,  // 传感器离线
    SENSOR_STATUS_ONLINE,       // 传感器在线正常
    SENSOR_STATUS_ERROR,        // 传感器错误
    SENSOR_STATUS_INITIALIZING  // 传感器初始化中
} SensorStatus_t;

/* --------------------------- 传感器数据结构 --------------------------- */
typedef struct {
    float lux;      // 光照强度 (单位: lux)
} SensorGY30Data_t;

typedef struct {
    float temp;     // 温度 (单位: °C)
    float humi;     // 湿度 (单位: %RH)
} SensorSHT30Data_t;

typedef struct {
    int ppm;        // 烟雾浓度 (单位: PPM)
} SensorSmokeData_t;
 
typedef struct {
    union {
        SensorGY30Data_t  gy30;     // 用于GY30传感器
        SensorSHT30Data_t sht30;    // 用于SHT30传感器
        SensorSmokeData_t smoke;    // 用于MQ-2烟雾传感器
        //... 可以继续扩展
    } values;
    uint32_t timestamp;         // 时间戳
    bool is_valid;              // 数据是否有效
} SensorData_t;

/* --------------------------- 传感器实例结构体 --------------------------- */
// [NEW] 1. 定义历史数据缓冲区的大小 (例如，存储最近60个点)
#define SENSOR_HISTORY_SIZE 20

// [NEW] 2. 定义用于存放统计数据的结构体
typedef struct {
    // 全局统计数据 (自启动以来)
    float min;
    float max;
    float avg;

    // 局部统计数据 (仅基于当前历史缓冲区)
    float local_min;
    float local_max;
    float local_avg;
} SensorStats_t;

typedef struct {
    SensorType_t type;                              // 传感器类型
    char name[SENSOR_MAX_NAME_LEN];                 // 传感器名称
    SensorStatus_t status;                          // 传感器状态
    SensorData_t data;                              // 传感器数据
    uint32_t update_interval_ms;                    // 更新间隔
    uint32_t last_update_time;                      // 上次更新时间
    uint32_t error_count;                           // 错误计数
    bool is_enabled;                                // 是否启用
    void* device_handle;                            // 设备句柄指针
        
    // [NEW] 3. 为每个传感器实例添加以下成员        
    SensorStats_t stats;                            // 统计数据 (最大/最小/平均值)
    SensorStats_t secondary_stats;                  // 备用统计数据 (如湿度)    
    float history[SENSOR_HISTORY_SIZE];             // 历史数据循环缓冲区
    float secondary_history[SENSOR_HISTORY_SIZE];   // 备用历史数据循环缓冲区（如湿度）
    uint16_t history_head;                          // 缓冲区的当前头部索引
    uint16_t history_count;                         // 记录已有的历史数据点数量
} SensorInstance_t;

/* --------------------------- 传感器回调函数类型 --------------------------- */
typedef struct {
    bool (*init_func)(SensorInstance_t* sensor);       // 初始化函数
    bool (*read_func)(SensorInstance_t* sensor);       // 读取函数
    bool (*deinit_func)(SensorInstance_t* sensor);     // 反初始化函数
    const char* (*get_unit)(void);                     // 获取单位字符串
} SensorCallbacks_t;

/* --------------------------- 传感器管理器结构体 --------------------------- */
typedef struct {
    SensorInstance_t sensors[SENSOR_TYPE_MAX];      // 传感器实例数组
    SensorCallbacks_t callbacks[SENSOR_TYPE_MAX];   // 回调函数数组
    bool is_initialized;                            // 管理器是否已初始化
    uint32_t active_sensor_count;                   // 活跃传感器数量

    osMutexId global_data_mutex;                    // [新增] 数据访问互斥锁
} SensorManager_t;

/* --------------------------- 传感器事件结构体 --------------------------- */
typedef enum {
    SENSOR_EVENT_DATA_UPDATE = 0,   // 数据更新事件
    SENSOR_EVENT_STATUS_CHANGE,     // 状态变化事件
    SENSOR_EVENT_ERROR              // 错误事件
} SensorEventType_t;

typedef struct {
    SensorEventType_t event_type;   // 事件类型
    SensorType_t sensor_type;       // 传感器类型
    SensorData_t data;              // 相关数据
    SensorStatus_t status;          // 相关状态
} SensorEvent_t;

/* --------------------------- 公共函数声明 --------------------------- */

/**
 * @brief 初始化传感器任务管理系统
 * @return true: 成功, false: 失败
 */
bool SensorTask_Init(void);

/**
 * @brief 注册传感器
 * @param type 传感器类型
 * @param name 传感器名称
 * @param callbacks 回调函数结构体
 * @param device_handle 设备句柄
 * @param update_interval_ms 更新间隔(毫秒)
 * @return true: 成功, false: 失败
 */
bool SensorTask_RegisterSensor(SensorType_t type, 
                              const char* name,
                              const SensorCallbacks_t* callbacks,
                              void* device_handle,
                              uint32_t update_interval_ms);

/**
 * @brief 启用传感器
 * @param type 传感器类型
 * @return true: 成功, false: 失败
 */
bool SensorTask_EnableSensor(SensorType_t type);

/**
 * @brief 禁用传感器
 * @param type 传感器类型
 * @return true: 成功, false: 失败
 */
bool SensorTask_DisableSensor(SensorType_t type);

/**
 * @brief 获取传感器数据
 * @param type 传感器类型
 * @param data 输出数据指针
 * @return true: 成功, false: 失败
 */
bool SensorTask_GetSensorData(SensorType_t type, SensorData_t* data);

/**
 * @brief 获取传感器状态
 * @param type 传感器类型
 * @param status 输出状态指针
 * @return true: 成功, false: 失败
 */
bool SensorTask_GetSensorStatus(SensorType_t type, SensorStatus_t* status);

/**
 * @brief 设置传感器更新间隔
 * @param type 传感器类型
 * @param interval_ms 更新间隔(毫秒)
 * @return true: 成功, false: 失败
 */
bool SensorTask_SetUpdateInterval(SensorType_t type, uint32_t interval_ms);

bool SensorTask_GetStats(SensorType_t type, SensorStats_t* stats);
bool SensorTask_GetSecondaryStats(SensorType_t type, SensorStats_t* stats); 
uint16_t SensorTask_GetPrimaryHistory(SensorType_t type, const float** history_data);
uint16_t SensorTask_GetSecondaryHistory(SensorType_t type, const float** history_data);

/**
 * @brief 获取传感器状态字符串
 * @param status 传感器状态
 * @return const char* 状态字符串
 */
const char* SensorStatus_ToString(SensorStatus_t status);

/**
 * @brief 获取传感器类型字符串
 * @param type 传感器类型
 * @return const char* 传感器类型字符串
 */
const char* SensorType_ToString(SensorType_t type);

/**
 * @brief 获取传感器事件类型字符串
 * @param event 传感器事件类型
 * @return const char* 事件类型字符串
 */
const char* SensorEventType_ToString(SensorEventType_t event);

/**
 * @brief 返回传感器任务的 FreeRTOS 句柄（只读）
 * @return osThreadId 传感器任务句柄，若未创建返回 NULL
 */
osThreadId SensorTask_GetHandle(void);

/* --------------------------- 事件通知相关 --------------------------- */

/**
 * @brief 注册传感器事件回调函数
 * @param callback 事件回调函数指针
 * @return true: 成功, false: 失败
 */
typedef void (*SensorEventCallback_t)(const SensorEvent_t* event);
bool SensorTask_RegisterEventCallback(SensorEventCallback_t callback);

/* --------------------------- 便利宏定义 --------------------------- */
#define SENSOR_DATA_IS_FRESH(sensor, max_age_ms) \
    ((HAL_GetTick() - (sensor)->data.timestamp) <= (max_age_ms))

#define SENSOR_IS_HEALTHY(sensor) \
    ((sensor)->status == SENSOR_STATUS_ONLINE && (sensor)->error_count < 5)

#define SENSOR_GET_AGE_MS(sensor) \
    (HAL_GetTick() - (sensor)->data.timestamp)


#ifdef  __cplusplus
}
#endif

#endif /* __SENSOR_TASK_H */
