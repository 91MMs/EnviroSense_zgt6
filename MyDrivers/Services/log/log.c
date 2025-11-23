#include "log.h"
#include "printf_redirect.h"
#include <string.h>
#include <time.h>

// 如果使用了printf_redirect系统，包含其头文件
#ifdef PRINTF_REDIRECT_H__
#include "printf_redirect.h"
#endif

// 默认配置
static log_config_t g_log_config = {
    .level = LOG_LEVEL_DEBUG,
    .show_timestamp = 0,
    .show_level = 1,
    .show_module = 1,
    .show_file_line = 0
};

// 日志级别字符串
static const char* log_level_strings[] = {
    "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"
};

// 日志级别颜色 (ANSI colors, 可选)
#if LOG_USE_COLOR
static const char* log_level_colors[] = {
    "\x1b[94m", // TRACE - 亮蓝色
    "\x1b[36m", // DEBUG - 青色
    "\x1b[32m", // INFO  - 绿色
    "\x1b[33m", // WARN  - 黄色
    "\x1b[31m", // ERROR - 红色
    "\x1b[35m"  // FATAL - 紫色
};
static const char* log_color_reset = "\x1b[0m";
#endif

// 获取文件名（去掉路径）
static const char* get_filename(const char* path) {
    const char* file = strrchr(path, '/');
    if (!file) {
        file = strrchr(path, '\\');
    }
    return file ? file + 1 : path;
}

// 获取时间戳字符串
static void get_timestamp(char* buffer, size_t size) {
#if FREERTOS_VERSION
    // FreeRTOS 环境下使用tick计数
    snprintf(buffer, size, "%lu", (unsigned long)xTaskGetTickCount());
#else
    // 标准C环境下使用时间
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, size, "%H:%M:%S", tm_info);
#endif
}

// 初始化日志系统
void log_init(void) {
    // 1. 如果使用了printf_redirect，确保已经初始化
    // printf_init(huart) 应该在main中已经调用
    
    // 2. 可以在这里输出初始化信息
    printf("[LOG] Log system initialized\r\n");
    printf("[LOG] Log level: %s\r\n", log_level_strings[g_log_config.level]);
    printf("[LOG] Features: timestamp=%s, level=%s, module=%s, file_line=%s\r\n",
           g_log_config.show_timestamp ? "ON" : "OFF",
           g_log_config.show_level ? "ON" : "OFF", 
           g_log_config.show_module ? "ON" : "OFF",
           g_log_config.show_file_line ? "ON" : "OFF");
    
    // 3. 强制刷新确保初始化信息输出
    printf_flush();
}

// 设置日志级别
void log_set_level(log_level_t level) {
    if (level <= LOG_LEVEL_OFF) {
        g_log_config.level = level;
    }
}

// 设置日志配置
void log_set_config(const log_config_t* config) {
    if (config) {
        g_log_config = *config;
    }
}

// 获取当前日志级别
log_level_t log_get_level(void) {
    return g_log_config.level;
}

// 核心日志输出函数
void log_write(log_level_t level, const char* module, const char* file, 
               int line, const char* fmt, ...) {
    
    // 检查日志级别
    if (level < g_log_config.level || level >= LOG_LEVEL_OFF) {
        return;
    }
    
    // 参数检查
    if (!fmt || !module) {
        return;
    }
    
    char timestamp[32] = {0};
    
    // 生成时间戳
    if (g_log_config.show_timestamp) {
        get_timestamp(timestamp, sizeof(timestamp));
    }
    
#if LOG_USE_COLOR
    // 使用颜色输出
    printf("%s", log_level_colors[level]);
#endif
    
    // 输出时间戳
    if (g_log_config.show_timestamp) {
        printf("[%s] ", timestamp);
    }
    
    // 输出日志级别
    if (g_log_config.show_level) {
        printf("[%s] ", log_level_strings[level]);
    }
    
    // 输出模块名
    if (g_log_config.show_module) {
        printf("[%s] ", module);
    }
    
    // 输出文件名和行号
    if (g_log_config.show_file_line && file) {
        printf("[%s:%d] ", get_filename(file), line);
    }
    
#if LOG_USE_COLOR
    printf("%s", log_color_reset);
#endif
    
    // 输出用户消息
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    printf("\r\n");
    
    // 立即刷新输出缓冲区
    fflush(stdout);
}
