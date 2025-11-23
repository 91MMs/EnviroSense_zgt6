/**
 * @file    ui_comp_header.h
 * @brief   可重用的页面顶部栏组件
 * @details 提供标准化的顶部栏，从左到右布局：
 *          返回按钮(可选) | 自定义按钮(可选) | 标题(居中) | 系统时间
 */

#ifndef UI_COMP_HEADER_H
#define UI_COMP_HEADER_H

#include "lvgl.h"
#include <stdbool.h>

/* 顶部栏配置结构体 */
typedef struct {
    const char* title;              /* 页面标题文本 */
    bool show_back_btn;             /* 是否显示返回按钮 */
    bool show_custom_btn;           /* 是否显示自定义按钮 */
    const char* custom_btn_text;    /* 自定义按钮文本 (如 "重置"、"保存") */
    lv_event_cb_t back_btn_cb;      /* 返回按钮回调函数 */
    lv_event_cb_t custom_btn_cb;    /* 自定义按钮回调函数 */
    void* user_data;                /* 用户自定义数据 (传递给回调) */
    bool show_time;                 /* 是否显示系统时间 (默认显示) */
} ui_header_config_t;

/* 顶部栏对象句柄 */
typedef struct {
    lv_obj_t* container;            /* 顶部栏容器 */
    lv_obj_t* back_btn;             /* 返回按钮 */
    lv_obj_t* custom_btn;           /* 自定义按钮 */
    lv_obj_t* title_label;          /* 标题标签 */
    lv_obj_t* time_label;           /* 时间标签 */
    lv_timer_t* time_update_timer;  /* 时间更新定时器 */
} ui_header_t;

/**
 * @brief 创建标准顶部栏
 * @param parent 父容器
 * @param config 顶部栏配置
 * @return 顶部栏对象句柄 (需要保存以便后续销毁)
 */
ui_header_t* ui_comp_header_create(lv_obj_t* parent, const ui_header_config_t* config);

/**
 * @brief 销毁顶部栏 (释放资源)
 * @param header 顶部栏句柄
 */
void ui_comp_header_destroy(ui_header_t* header);

/**
 * @brief 更新标题文本
 * @param header 顶部栏句柄
 * @param title 新标题文本
 */
void ui_comp_header_set_title(ui_header_t* header, const char* title);

/**
 * @brief 手动更新时间显示 (通常由定时器自动调用)
 * @param header 顶部栏句柄
 */
void ui_comp_header_update_time(ui_header_t* header);

#endif /* UI_COMP_HEADER_H */
