/**
 ******************************************************************************
 * @file    log.h
 * @brief   日志系统头文件
 * @details 定义了日志级别、配置结构体及宏接口。
 * @author  MmsY
 * @time    2025/11/23
 ******************************************************************************
 */

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

// 日志级别定义
typedef enum {
  LOG_LEVEL_TRACE = 0,
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_FATAL,
  LOG_LEVEL_OFF
} log_level_t;

// 日志配置结构体
typedef struct {
  log_level_t level;  // 当前日志级别
  int show_timestamp; // 是否显示时间戳
  int show_level;     // 是否显示日志级别
  int show_module;    // 是否显示模块名
  int show_file_line; // 是否显示文件名和行号
} log_config_t;

// 日志系统初始化和配置
void log_init(void);
void log_set_level(log_level_t level);
void log_set_config(const log_config_t *config);
log_level_t log_get_level(void);

// 核心日志函数
void log_write(log_level_t level, const char *module, const char *file,
               int line, const char *fmt, ...);

// 便捷宏定义
#ifndef LOG_MODULE
#define LOG_MODULE "UNKNOWN"
#endif

// 额外配置
#define LOG_USE_COLOR 0
#define FREERTOS_VERSION 1

// 基础日志宏
#define LOG_TRACE(fmt, ...)                                                    \
  log_write(LOG_LEVEL_TRACE, LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)                                                    \
  log_write(LOG_LEVEL_DEBUG, LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)                                                     \
  log_write(LOG_LEVEL_INFO, LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)                                                     \
  log_write(LOG_LEVEL_WARN, LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)                                                    \
  log_write(LOG_LEVEL_ERROR, LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)                                                    \
  log_write(LOG_LEVEL_FATAL, LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// 兼容你原来的宏定义风格
#define LOG(fmt, ...) LOG_INFO(fmt, ##__VA_ARGS__)

// 条件编译支持（保持向后兼容）
#ifndef LOG_ENABLE
#define LOG_ENABLE 1
#endif

#if !LOG_ENABLE
#undef LOG_TRACE
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARN
#undef LOG_ERROR
#undef LOG_FATAL
#undef LOG
#define LOG_TRACE(fmt, ...)
#define LOG_DEBUG(fmt, ...)
#define LOG_INFO(fmt, ...)
#define LOG_WARN(fmt, ...)
#define LOG_ERROR(fmt, ...)
#define LOG_FATAL(fmt, ...)
#define LOG(fmt, ...)
#endif

// 简化的模块专用宏（可选使用）
#define DECLARE_LOG_MODULE(module_name)                                        \
  static const char *__log_module = #module_name;                              \
  static inline void module_log_trace(const char *fmt, ...) {                  \
    va_list args;                                                              \
    va_start(args, fmt);                                                       \
    log_write(LOG_LEVEL_TRACE, __log_module, __FILE__, __LINE__, fmt, args);   \
    va_end(args);                                                              \
  }                                                                            \
  static inline void module_log_debug(const char *fmt, ...) {                  \
    va_list args;                                                              \
    va_start(args, fmt);                                                       \
    log_write(LOG_LEVEL_DEBUG, __log_module, __FILE__, __LINE__, fmt, args);   \
    va_end(args);                                                              \
  }                                                                            \
  static inline void module_log_info(const char *fmt, ...) {                   \
    va_list args;                                                              \
    va_start(args, fmt);                                                       \
    log_write(LOG_LEVEL_INFO, __log_module, __FILE__, __LINE__, fmt, args);    \
    va_end(args);                                                              \
  }                                                                            \
  static inline void module_log_warn(const char *fmt, ...) {                   \
    va_list args;                                                              \
    va_start(args, fmt);                                                       \
    log_write(LOG_LEVEL_WARN, __log_module, __FILE__, __LINE__, fmt, args);    \
    va_end(args);                                                              \
  }                                                                            \
  static inline void module_log_error(const char *fmt, ...) {                  \
    va_list args;                                                              \
    va_start(args, fmt);                                                       \
    log_write(LOG_LEVEL_ERROR, __log_module, __FILE__, __LINE__, fmt, args);   \
    va_end(args);                                                              \
  }                                                                            \
  static inline void module_log_fatal(const char *fmt, ...) {                  \
    va_list args;                                                              \
    va_start(args, fmt);                                                       \
    log_write(LOG_LEVEL_FATAL, __log_module, __FILE__, __LINE__, fmt, args);   \
    va_end(args);                                                              \
  }

#ifdef __cplusplus
}
#endif

#endif // LOG_H
