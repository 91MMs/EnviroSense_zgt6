/**
 * @file    ui_comp_header.c
 * @brief   可重用的页面顶部栏组件实现
 */

#include "ui_comp_header.h"
#include <stdio.h>
#include <time.h>

/* 声明外部字体 */
LV_FONT_DECLARE(my_font_yahei_24);

/* 顶部栏高度 */
#define HEADER_HEIGHT 70

/* 顶部栏背景色 */
#define HEADER_BG_COLOR 0xF5EFE6

/* 时间更新定时器周期 (ms) */
#define TIME_UPDATE_PERIOD_MS 1000

/* -----------------------------------------------------------
 * 内部函数声明
 * ----------------------------------------------------------- */
static void time_update_timer_cb(lv_timer_t* timer);

/* -----------------------------------------------------------
 * 时间更新定时器回调
 * ----------------------------------------------------------- */
static void time_update_timer_cb(lv_timer_t* timer)
{
    ui_header_t* header = (ui_header_t*)timer->user_data;
    if (header && header->time_label) {
        ui_comp_header_update_time(header);
    }
}

/* -----------------------------------------------------------
 * 公共 API 实现
 * ----------------------------------------------------------- */

/**
 * @brief 创建标准顶部栏
 */
ui_header_t* ui_comp_header_create(lv_obj_t* parent, const ui_header_config_t* config)
{
    /* 分配句柄内存 */
    ui_header_t* header = (ui_header_t*)lv_mem_alloc(sizeof(ui_header_t));
    if (!header) return NULL;
    memset(header, 0, sizeof(ui_header_t));

    /* === 1. 创建顶部栏容器 === */
    header->container = lv_obj_create(parent);
    lv_obj_remove_style_all(header->container);
    lv_obj_set_size(header->container, LV_PCT(100), HEADER_HEIGHT);
    lv_obj_set_style_bg_color(header->container, lv_color_hex(HEADER_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(header->container, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(header->container, 10, 0); /* 左右内边距 */
    
    /* 使用 Flex 布局 (水平排列) */
    lv_obj_set_flex_flow(header->container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header->container, 
                          LV_FLEX_ALIGN_SPACE_BETWEEN,  /* 主轴两端对齐 */
                          LV_FLEX_ALIGN_CENTER,         /* 交叉轴居中 */
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(header->container, 10, 0); /* 子元素间距 */

    /* === 2. 创建左侧容器 (返回按钮 + 自定义按钮) === */
    lv_obj_t* left_container = lv_obj_create(header->container);
    lv_obj_remove_style_all(left_container);
    lv_obj_set_size(left_container, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(left_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(left_container, 10, 0);

    /* 2.1 返回按钮 (可选) */
    if (config->show_back_btn) {
        header->back_btn = lv_btn_create(left_container);
        lv_obj_set_size(header->back_btn, 80, 40);
        
        lv_obj_t* back_label = lv_label_create(header->back_btn);
        lv_label_set_text(back_label, LV_SYMBOL_LEFT " 返回");
        lv_obj_set_style_text_font(back_label, &my_font_yahei_24, 0);
        lv_obj_center(back_label);
        
        if (config->back_btn_cb) {
            lv_obj_add_event_cb(header->back_btn, config->back_btn_cb, LV_EVENT_CLICKED, config->user_data);
        }
    }

    /* 2.2 自定义按钮 (可选) */
    if (config->show_custom_btn && config->custom_btn_text) {
        header->custom_btn = lv_btn_create(left_container);
        lv_obj_set_size(header->custom_btn, 80, 40);
        
        lv_obj_t* custom_label = lv_label_create(header->custom_btn);
        lv_label_set_text(custom_label, config->custom_btn_text);
        lv_obj_set_style_text_font(custom_label, &my_font_yahei_24, 0);
        lv_obj_center(custom_label);
        
        if (config->custom_btn_cb) {
            lv_obj_add_event_cb(header->custom_btn, config->custom_btn_cb, LV_EVENT_CLICKED, config->user_data);
        }
    }

    /* === 3. 创建标题 (居中) === */
    header->title_label = lv_label_create(header->container);
    lv_label_set_text(header->title_label, config->title ? config->title : "未命名页面");
    lv_obj_set_style_text_font(header->title_label, &my_font_yahei_24, 0);
    lv_obj_set_flex_grow(header->title_label, 1); /* 占据剩余空间 */
    lv_obj_set_style_text_align(header->title_label, LV_TEXT_ALIGN_CENTER, 0);

    /* === 4. 创建时间标签 (右侧) === */
    if (config->show_time) {
        header->time_label = lv_label_create(header->container);
        lv_label_set_text(header->time_label, "--:--:--");
        lv_obj_set_style_text_font(header->time_label, &lv_font_montserrat_20, 0);
        lv_obj_set_width(header->time_label, 100); /* 固定宽度，避免抖动 */
        lv_obj_set_style_text_align(header->time_label, LV_TEXT_ALIGN_RIGHT, 0);
        
        /* 立即更新一次时间 */
        ui_comp_header_update_time(header);
        
        /* 创建定时器以定期更新时间 */
        header->time_update_timer = lv_timer_create(time_update_timer_cb, TIME_UPDATE_PERIOD_MS, header);
    }

    return header;
}

/**
 * @brief 销毁顶部栏
 */
void ui_comp_header_destroy(ui_header_t* header)
{
    if (!header) return;
    
    /* 删除定时器 */
    if (header->time_update_timer) {
        lv_timer_del(header->time_update_timer);
        header->time_update_timer = NULL;
    }
    
    /* 删除容器 (会自动删除所有子对象) */
    if (header->container) {
        lv_obj_del(header->container);
    }
    
    /* 释放句柄内存 */
    lv_mem_free(header);
}

/**
 * @brief 更新标题文本
 */
void ui_comp_header_set_title(ui_header_t* header, const char* title)
{
    if (header && header->title_label) {
        lv_label_set_text(header->title_label, title ? title : "");
    }
}

/**
 * @brief 更新时间显示
 * @note 需要根据你的系统实现获取时间的逻辑
 */
void ui_comp_header_update_time(ui_header_t* header)
{
    if (!header || !header->time_label) return;
    
    /* 方法1: 使用 C 标准库 time.h (如果支持) */
    #if 0
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    if (tm_info) {
        lv_label_set_text_fmt(header->time_label, "%02d:%02d:%02d", 
                              tm_info->tm_hour, 
                              tm_info->tm_min, 
                              tm_info->tm_sec);
    }
    #else
    /* 方法2: 如果没有 RTC，可以使用 lv_tick_get() 模拟 */
    uint32_t tick_ms = lv_tick_get();
    uint32_t total_seconds = tick_ms / 1000;
    uint8_t hours = (total_seconds / 3600) % 24;
    uint8_t minutes = (total_seconds / 60) % 60;
    uint8_t seconds = total_seconds % 60;
    lv_label_set_text_fmt(header->time_label, "%02d:%02d:%02d", hours, minutes, seconds);
    #endif
}
