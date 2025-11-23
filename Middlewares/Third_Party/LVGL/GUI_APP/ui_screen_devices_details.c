/**
 ******************************************************************************
 * @file    ui_screen_devices_details.c
 * @brief   设备详细设置页面实现（整理版）
 ******************************************************************************
 */

#include "ui_screen_devices_details.h"
#include "ui_manager.h"
#include "devices_manager.h"
#include "ui_comp_header.h"
#include "lvgl.h"

/* 字体声明（如需） */
LV_FONT_DECLARE(my_font_yahei_24);

/* UI 状态结构体 */
typedef struct {
    lv_obj_t * r_slider[3];
    lv_obj_t * g_slider[3];
    lv_obj_t * b_slider[3];
    lv_obj_t * r_value_label[3];
    lv_obj_t * g_value_label[3];
    lv_obj_t * b_value_label[3];
    lv_obj_t * brightness_slider;
    lv_obj_t * brightness_panel;

    lv_obj_t * preview_led[3];
    lv_obj_t * slot_label[3];
    lv_obj_t * slot_panel[3];
    lv_obj_t * mode_switch_btn;
    lv_obj_t * content_panel;
    uint8_t current_editing_slot;
    bool is_auto_mode;
} rgbled_details_ui_t;

static rgbled_details_ui_t g_rgbled_ui;
static ui_header_t* g_header = NULL;

/* 函数声明（按实现顺序） */
static void set_led_border_smart(lv_obj_t* led, RGB_Color color);
static void update_led_display(uint8_t slot, bool is_active);
static void refresh_ui_for_mode(void);
static void led_click_event_cb(lv_event_t *e);
static void rgb_slider_event_cb(lv_event_t *e);
static void rgb_slider_value_update_cb(lv_event_t *e);
static void brightness_slider_event_cb(lv_event_t *e);
static void brightness_value_update_cb(lv_event_t *e);
static void reset_btn_event_cb(lv_event_t *e);
static void back_btn_event_cb(lv_event_t *e);
static void slot_panel_click_event_cb(lv_event_t *e);
static void mode_switch_btn_event_cb(lv_event_t *e);
static void create_rgb_slider_group(lv_obj_t* parent, uint8_t slot_index);
static void init_ui_from_driver_state(void);
static void create_rgbled_details_ui(lv_obj_t* parent);

/* -------------------- 帮助函数 -------------------- */

/* 根据颜色设置合适边框颜色，保证在不同背景下可见 */
static void set_led_border_smart(lv_obj_t* led, RGB_Color color)
{
    uint16_t lum = (color.R * 299 + color.G * 587 + color.B * 114) / 1000;
    if (lum > 180) {
        lv_obj_set_style_border_color(led, lv_color_hex(0x444444), 0);
    } else {
        lv_obj_set_style_border_color(led, lv_color_hex(0xAAAAAA), 0);
    }
}

/* 更新单个槽位的预览显示
   - 背景色显示槽位颜色（始终）
   - 高光（白色）表示该槽位为当前激活槽位 */
static void update_led_display(uint8_t slot, bool is_active)
{
    if (slot < 1 || slot > 3) return;
    lv_obj_t* led = g_rgbled_ui.preview_led[slot - 1];
    RGB_Color color;
    if (!Drivers_RGBLED_GetSlotColor(slot, &color)) {
        return;
    }

    /* 背景色：显示槽位颜色 */
    lv_obj_set_style_bg_color(led, lv_color_make(color.R, color.G, color.B), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(led, LV_OPA_COVER, LV_PART_MAIN);

    /* 边框 */
    set_led_border_smart(led, color);
    lv_obj_set_style_border_width(led, 3, 0);

    /* 高光表示激活 */
    if (is_active) {
        lv_led_set_color(led, lv_color_make(0xFF, 0x98, 0x5E));  // 橙色高光
        lv_led_on(led);
    } else {
        lv_led_off(led);
    }
}

/* -------------------- 事件回调 -------------------- */

/* LED 预览点击事件：切换激活槽位或关闭物理 LED */
static void led_click_event_cb(lv_event_t *e)
{
    uint8_t slot = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (slot < 1 || slot > 3) return;
    
    /* 只在手动模式下生效 */
    if (g_rgbled_ui.is_auto_mode) {
        return;
    }
    
    /* 获取当前激活槽位 */
    uint8_t current_active = g_rgbled_ui.current_editing_slot;
    
    if (slot == current_active) {
        /* === 点击当前激活槽位的 LED：关闭物理 LED === */
        Drivers_RGBLED_Off();  // 关闭物理 LED，槽位状态变为 LED_STATE_OFF
        
        /* 取消所有槽位的 LED 高光 */
        for (uint8_t i = 0; i < 3; i++) {
            update_led_display(i + 1, false);
        }
        
        /* 取消面板边框 */
        lv_obj_set_style_border_width(g_rgbled_ui.slot_panel[slot - 1], 0, 0);
        lv_obj_clear_state(g_rgbled_ui.slot_label[slot - 1], LV_STATE_FOCUSED);
        
        g_rgbled_ui.current_editing_slot = 0;  // 标记为无激活槽位
        
    } else {
        /* === 点击其他槽位的 LED：切换激活槽位 === */
        g_rgbled_ui.current_editing_slot = slot;
        
        /* 更新所有槽位的显示和边框 */
        for (uint8_t i = 0; i < 3; i++) {
            if (i == slot - 1) {
                lv_obj_set_style_border_color(g_rgbled_ui.slot_panel[i], lv_color_hex(0xF8F7BA), 0);
                lv_obj_set_style_border_width(g_rgbled_ui.slot_panel[i], 4, 0);
                lv_obj_set_style_border_opa(g_rgbled_ui.slot_panel[i], LV_OPA_100, 0);
                lv_obj_add_state(g_rgbled_ui.slot_label[i], LV_STATE_FOCUSED);
            } else {
                lv_obj_set_style_border_width(g_rgbled_ui.slot_panel[i], 0, 0);
                lv_obj_clear_state(g_rgbled_ui.slot_label[i], LV_STATE_FOCUSED);
            }
            update_led_display(i + 1, (i + 1 == slot));
        }
        
        /* 开启物理 LED */
        Drivers_RGBLED_SetManualSlot(slot);
    }
}

/* RGB 滑块：改变槽位颜色（只改变槽位颜色与背景，按当前激活槽位更新物理灯） */
static void rgb_slider_event_cb(lv_event_t *e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    uint32_t data = (uint32_t)(intptr_t)lv_event_get_user_data(e);
    uint8_t slot_index = (data >> 8) & 0xFF;
    uint8_t channel = data & 0xFF;
    uint8_t slot = slot_index + 1;
    int32_t value = lv_slider_get_value(slider);

    RGB_Color color;
    if (!Drivers_RGBLED_GetSlotColor(slot, &color)) return;

    if (channel == 0) color.R = (uint8_t)value;
    else if (channel == 1) color.G = (uint8_t)value;
    else if (channel == 2) color.B = (uint8_t)value;
    else return;

    if (Drivers_RGBLED_SetSlotColor(slot, color)) {
        /* 更新 UI 背景色 */
        lv_obj_t* led = g_rgbled_ui.preview_led[slot_index];
        lv_obj_set_style_bg_color(led, lv_color_make(color.R, color.G, color.B), LV_PART_MAIN);
        set_led_border_smart(led, color);

        /* 若自动模式且为槽位1，或手动且为当前激活槽位，更新物理 LED */
        if ((g_rgbled_ui.is_auto_mode && slot == 1) ||
            (!g_rgbled_ui.is_auto_mode && g_rgbled_ui.current_editing_slot == slot)) {
            Drivers_RGBLED_SetColor(color);
        }
    }
}

/* 滑块数值显示更新 */
static void rgb_slider_value_update_cb(lv_event_t *e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
    int32_t value = lv_slider_get_value(slider);
    lv_label_set_text_fmt(label, "%d", (int)value);
}

/* 亮度滑块回调 */
static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    Drivers_RGBLED_SetBrightness((uint8_t)value);
}

/* 亮度数值显示更新 */
static void brightness_value_update_cb(lv_event_t *e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
    int32_t value = lv_slider_get_value(slider);
    lv_label_set_text_fmt(label, "%d", (int)value);
}

/* 重置按钮（恢复驱动层默认并刷新 UI） */
static void reset_btn_event_cb(lv_event_t *e)
{
    // 如果为手动模式
    if (!g_rgbled_ui.is_auto_mode) {
        Drivers_RGBLED_ResetSlotColors();
        g_rgbled_ui.current_editing_slot = 1;
        
        RGB_Color color;
        for (uint8_t i = 1; i <= 3; i++) {
            if (Drivers_RGBLED_GetSlotColor(i, &color)) {
                lv_slider_set_value(g_rgbled_ui.r_slider[i - 1], color.R, LV_ANIM_OFF);
                lv_slider_set_value(g_rgbled_ui.g_slider[i - 1], color.G, LV_ANIM_OFF);
                lv_slider_set_value(g_rgbled_ui.b_slider[i - 1], color.B, LV_ANIM_OFF);

                lv_label_set_text_fmt(g_rgbled_ui.r_value_label[i - 1], "%d", color.R);
                lv_label_set_text_fmt(g_rgbled_ui.g_value_label[i - 1], "%d", color.G);
                lv_label_set_text_fmt(g_rgbled_ui.b_value_label[i - 1], "%d", color.B);

                if (i == 1) {
                    /* 槽位1：添加高亮 */
                    lv_obj_set_style_border_color(g_rgbled_ui.slot_panel[0], lv_color_hex(0xF8F7BA), 0);
                    lv_obj_set_style_border_width(g_rgbled_ui.slot_panel[0], 4, 0);
                    lv_obj_set_style_border_opa(g_rgbled_ui.slot_panel[0], LV_OPA_100, 0);
                    lv_obj_add_state(g_rgbled_ui.slot_label[0], LV_STATE_FOCUSED);
                    update_led_display(1, true);  // LED 高光
                } else {
                    /* 槽位2/3：取消高亮 */
                    lv_obj_set_style_border_width(g_rgbled_ui.slot_panel[i - 1], 0, 0);
                    lv_obj_clear_state(g_rgbled_ui.slot_label[i - 1], LV_STATE_FOCUSED);
                    update_led_display(i, false);  // 无高光
                }
            }
        }
        Drivers_RGBLED_SetManualSlot(1);
        
    } else {
        // 如果为自动模式
        Drivers_RGBLED_ResetSlotColors();

        RGB_Color color;
        if (Drivers_RGBLED_GetSlotColor(1, &color)) {
            lv_slider_set_value(g_rgbled_ui.r_slider[0], color.R, LV_ANIM_OFF);
            lv_slider_set_value(g_rgbled_ui.g_slider[0], color.G, LV_ANIM_OFF);
            lv_slider_set_value(g_rgbled_ui.b_slider[0], color.B, LV_ANIM_OFF);

            lv_label_set_text_fmt(g_rgbled_ui.r_value_label[0], "%d", color.R);
            lv_label_set_text_fmt(g_rgbled_ui.g_value_label[0], "%d", color.G);
            lv_label_set_text_fmt(g_rgbled_ui.b_value_label[0], "%d", color.B);

            /* 槽位1高亮 */
            lv_obj_set_style_border_color(g_rgbled_ui.slot_panel[0], lv_color_hex(0xF8F7BA), 0);
            lv_obj_set_style_border_width(g_rgbled_ui.slot_panel[0], 4, 0);
            lv_obj_set_style_border_opa(g_rgbled_ui.slot_panel[0], LV_OPA_100, 0);
            lv_obj_add_state(g_rgbled_ui.slot_label[0], LV_STATE_FOCUSED);
            update_led_display(1, true);
        }
    }
}

/* 返回按钮 */
static void back_btn_event_cb(lv_event_t *e)
{
    ui_load_previous_screen();
}

/* 槽位面板点击：切换激活槽位（仅一个槽位高光） */
static void slot_panel_click_event_cb(lv_event_t *e)
{
    uint8_t slot = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (slot < 1 || slot > 3) return;

    g_rgbled_ui.current_editing_slot = slot;

    for (uint8_t i = 0; i < 3; i++) {
        if (i == slot - 1) {
            lv_obj_add_state(g_rgbled_ui.slot_label[i], LV_STATE_FOCUSED);

            lv_obj_set_style_border_color(g_rgbled_ui.slot_panel[i], lv_color_hex(0xF8F7BA), 0);    // 黄色边框
            lv_obj_set_style_border_width(g_rgbled_ui.slot_panel[i], 4, 0);                         // 4px 粗边框
            lv_obj_set_style_border_opa(g_rgbled_ui.slot_panel[i], LV_OPA_100, 0);                  // 完全不透明
        } else {
            lv_obj_clear_state(g_rgbled_ui.slot_label[i], LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(g_rgbled_ui.slot_panel[i], 0, 0);
        }
        update_led_display(i + 1, (i + 1 == slot));
    }

    if (!g_rgbled_ui.is_auto_mode) {
        Drivers_RGBLED_SetManualSlot(slot);
    }
}

/* 模式切换（自动 <-> 手动） */
static void mode_switch_btn_event_cb(lv_event_t *e)
{
    led_control_mode_t current_mode = Drivers_RGBLED_GetMode();
    if (current_mode == LED_MODE_MANUAL) {
        Drivers_RGBLED_SetMode(LED_MODE_AUTO);
        g_rgbled_ui.is_auto_mode = true;
        /* 激活槽位1 */
        for (uint8_t i = 0; i < 3; i++) update_led_display(i + 1, (i == 0));
    } else {
        Drivers_RGBLED_SetMode(LED_MODE_MANUAL);
        g_rgbled_ui.is_auto_mode = false;
        led_manual_state_t s = Drivers_RGBLED_GetManualState();
        uint8_t active_slot = (s == LED_STATE_SLOT_2) ? 2 : (s == LED_STATE_SLOT_3 ? 3 : 1);
        for (uint8_t i = 0; i < 3; i++) update_led_display(i + 1, (i + 1 == active_slot));
    }

    refresh_ui_for_mode();
}

/* -------------------- UI 创建相关 -------------------- */

/* 创建单个槽位的 RGB 滑块组 */
static void create_rgb_slider_group(lv_obj_t* parent, uint8_t slot_index)
{
    RGB_Color color;
    Drivers_RGBLED_GetSlotColor(slot_index + 1, &color);

    /* 容器布局：R 行 */
    lv_obj_t * r_container = lv_obj_create(parent);
    lv_obj_remove_style_all(r_container);
    lv_obj_set_size(r_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(r_container, 12, 0);

    lv_obj_t * r_label = lv_label_create(r_container);
    lv_label_set_text(r_label, "R");
    lv_obj_set_style_text_color(r_label, lv_color_hex(0xFF0000), 0);
    lv_obj_set_width(r_label, 18);

    g_rgbled_ui.r_slider[slot_index] = lv_slider_create(r_container);
    lv_obj_set_flex_grow(g_rgbled_ui.r_slider[slot_index], 1);
    lv_obj_set_height(g_rgbled_ui.r_slider[slot_index], 12);
    lv_slider_set_range(g_rgbled_ui.r_slider[slot_index], 0, 255);
    lv_slider_set_value(g_rgbled_ui.r_slider[slot_index], color.R, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_rgbled_ui.r_slider[slot_index], lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    uint32_t r_data = (slot_index << 8) | 0;
    lv_obj_add_event_cb(g_rgbled_ui.r_slider[slot_index], rgb_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)r_data);

    g_rgbled_ui.r_value_label[slot_index] = lv_label_create(r_container);
    lv_label_set_text_fmt(g_rgbled_ui.r_value_label[slot_index], "%d", color.R);
    lv_obj_set_width(g_rgbled_ui.r_value_label[slot_index], 36);
    lv_obj_add_event_cb(g_rgbled_ui.r_slider[slot_index], rgb_slider_value_update_cb, LV_EVENT_VALUE_CHANGED, g_rgbled_ui.r_value_label[slot_index]);

    /* G 行 */
    lv_obj_t * g_container = lv_obj_create(parent);
    lv_obj_remove_style_all(g_container);
    lv_obj_set_size(g_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(g_container, 12, 0);

    lv_obj_t * g_label = lv_label_create(g_container);
    lv_label_set_text(g_label, "G");
    lv_obj_set_style_text_color(g_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_width(g_label, 18);

    g_rgbled_ui.g_slider[slot_index] = lv_slider_create(g_container);
    lv_obj_set_flex_grow(g_rgbled_ui.g_slider[slot_index], 1);
    lv_obj_set_height(g_rgbled_ui.g_slider[slot_index], 12);
    lv_slider_set_range(g_rgbled_ui.g_slider[slot_index], 0, 255);
    lv_slider_set_value(g_rgbled_ui.g_slider[slot_index], color.G, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_rgbled_ui.g_slider[slot_index], lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    uint32_t g_data = (slot_index << 8) | 1;
    lv_obj_add_event_cb(g_rgbled_ui.g_slider[slot_index], rgb_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)g_data);

    g_rgbled_ui.g_value_label[slot_index] = lv_label_create(g_container);
    lv_label_set_text_fmt(g_rgbled_ui.g_value_label[slot_index], "%d", color.G);
    lv_obj_set_width(g_rgbled_ui.g_value_label[slot_index], 36);
    lv_obj_add_event_cb(g_rgbled_ui.g_slider[slot_index], rgb_slider_value_update_cb, LV_EVENT_VALUE_CHANGED, g_rgbled_ui.g_value_label[slot_index]);

    /* B 行 */
    lv_obj_t * b_container = lv_obj_create(parent);
    lv_obj_remove_style_all(b_container);
    lv_obj_set_size(b_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(b_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(b_container, 12, 0);

    lv_obj_t * b_label = lv_label_create(b_container);
    lv_label_set_text(b_label, "B");
    lv_obj_set_style_text_color(b_label, lv_color_hex(0x0000FF), 0);
    lv_obj_set_width(b_label, 18);

    g_rgbled_ui.b_slider[slot_index] = lv_slider_create(b_container);
    lv_obj_set_flex_grow(g_rgbled_ui.b_slider[slot_index], 1);
    lv_obj_set_height(g_rgbled_ui.b_slider[slot_index], 12);
    lv_slider_set_range(g_rgbled_ui.b_slider[slot_index], 0, 255);
    lv_slider_set_value(g_rgbled_ui.b_slider[slot_index], color.B, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_rgbled_ui.b_slider[slot_index], lv_color_hex(0x0000FF), LV_PART_INDICATOR);
    uint32_t b_data = (slot_index << 8) | 2;
    lv_obj_add_event_cb(g_rgbled_ui.b_slider[slot_index], rgb_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)b_data);

    g_rgbled_ui.b_value_label[slot_index] = lv_label_create(b_container);
    lv_label_set_text_fmt(g_rgbled_ui.b_value_label[slot_index], "%d", color.B);
    lv_obj_set_width(g_rgbled_ui.b_value_label[slot_index], 36);
    lv_obj_add_event_cb(g_rgbled_ui.b_slider[slot_index], rgb_slider_value_update_cb, LV_EVENT_VALUE_CHANGED, g_rgbled_ui.b_value_label[slot_index]);
}

/* 根据当前模式显示/隐藏面板元素 */
static void refresh_ui_for_mode(void)
{
    if (g_rgbled_ui.is_auto_mode) {
        lv_obj_add_flag(g_rgbled_ui.slot_panel[1], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_rgbled_ui.slot_panel[2], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_rgbled_ui.brightness_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(g_rgbled_ui.slot_panel[0], LV_PCT(98));
        lv_label_set_text(g_rgbled_ui.slot_label[0], "自动调光");
    } else {
        lv_obj_clear_flag(g_rgbled_ui.slot_panel[1], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_rgbled_ui.slot_panel[2], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_rgbled_ui.brightness_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(g_rgbled_ui.slot_panel[0], LV_PCT(31));
        lv_label_set_text(g_rgbled_ui.slot_label[0], "颜色 1");
    }
}

/* 初始化 UI（从驱动层读取状态并同步） */
static void init_ui_from_driver_state(void)
{
    led_control_mode_t mode = Drivers_RGBLED_GetMode();
    g_rgbled_ui.is_auto_mode = (mode == LED_MODE_AUTO);

    refresh_ui_for_mode();

    uint8_t active_slot = 1;
    if (mode == LED_MODE_MANUAL) {
        led_manual_state_t s = Drivers_RGBLED_GetManualState();
        switch (s) {
            case LED_STATE_SLOT_1:
                active_slot = 1;
                break;
            case LED_STATE_SLOT_2:
                active_slot = 2;
                break;
            case LED_STATE_SLOT_3:
                active_slot = 3;
                break;
            case LED_STATE_OFF:
            default:
                active_slot = 0;  // 无激活槽位
                break;
        }
    } else {
        active_slot = 1; /* 自动模式：默认激活槽位1 */
    }

    for (uint8_t i = 0; i < 3; i++) {
        RGB_Color color;
        if (!Drivers_RGBLED_GetSlotColor(i + 1, &color)) continue;

        lv_slider_set_value(g_rgbled_ui.r_slider[i], color.R, LV_ANIM_OFF);
        lv_slider_set_value(g_rgbled_ui.g_slider[i], color.G, LV_ANIM_OFF);
        lv_slider_set_value(g_rgbled_ui.b_slider[i], color.B, LV_ANIM_OFF);

        lv_label_set_text_fmt(g_rgbled_ui.r_value_label[i], "%d", color.R);
        lv_label_set_text_fmt(g_rgbled_ui.g_value_label[i], "%d", color.G);
        lv_label_set_text_fmt(g_rgbled_ui.b_value_label[i], "%d", color.B);

        /* [NEW] 根据 active_slot 更新显示状态 */
        bool is_active = (active_slot > 0 && (i + 1 == active_slot));
        
        if (is_active) {
            /* 激活槽位：添加边框 + LED 高光 */
            lv_obj_set_style_border_color(g_rgbled_ui.slot_panel[i], lv_color_hex(0xF8F7BA), 0);
            lv_obj_set_style_border_width(g_rgbled_ui.slot_panel[i], 4, 0);
            lv_obj_set_style_border_opa(g_rgbled_ui.slot_panel[i], LV_OPA_100, 0);
            lv_obj_add_state(g_rgbled_ui.slot_label[i], LV_STATE_FOCUSED);
        } else {
            /* 未激活槽位：无边框 + 无高光 */
            lv_obj_set_style_border_width(g_rgbled_ui.slot_panel[i], 0, 0);
            lv_obj_clear_state(g_rgbled_ui.slot_label[i], LV_STATE_FOCUSED);
        }
        
        update_led_display(i + 1, is_active);
    }

    g_rgbled_ui.current_editing_slot = active_slot;
}

/* 主界面创建 */
static void create_rgbled_details_ui(lv_obj_t* parent)
{
    memset(&g_rgbled_ui, 0, sizeof(rgbled_details_ui_t));
    g_rgbled_ui.current_editing_slot = 1;

    led_control_mode_t mode = Drivers_RGBLED_GetMode();
    g_rgbled_ui.is_auto_mode = (mode == LED_MODE_AUTO);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_gap(parent, 8, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* header */
    ui_header_config_t header_config = {
        .title = "RGB LED 设置",
        .show_back_btn = true,
        .show_custom_btn = true,
        .custom_btn_text = "重置",
        .back_btn_cb = back_btn_event_cb,
        .custom_btn_cb = reset_btn_event_cb,
        .user_data = NULL,
        .show_time = true
    };
    g_header = ui_comp_header_create(parent, &header_config);

    /* 模式切换按钮 */
    lv_obj_t * mode_btn_container = lv_obj_create(parent);
    lv_obj_remove_style_all(mode_btn_container);
    lv_obj_set_size(mode_btn_container, LV_PCT(100), 50);
    lv_obj_set_flex_flow(mode_btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mode_btn_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_rgbled_ui.mode_switch_btn = lv_btn_create(mode_btn_container);
    lv_obj_set_size(g_rgbled_ui.mode_switch_btn, 180, 40);
    lv_obj_set_style_bg_color(g_rgbled_ui.mode_switch_btn, lv_palette_main(LV_PALETTE_BLUE), 0);

    lv_obj_t * mode_btn_label = lv_label_create(g_rgbled_ui.mode_switch_btn);
    if (g_rgbled_ui.is_auto_mode) lv_label_set_text(mode_btn_label, LV_SYMBOL_REFRESH " 自动模式");
    else lv_label_set_text(mode_btn_label, LV_SYMBOL_SETTINGS " 手动模式");
    lv_obj_set_style_text_font(mode_btn_label, &my_font_yahei_24, 0);
    lv_obj_center(mode_btn_label);
    lv_obj_add_event_cb(g_rgbled_ui.mode_switch_btn, mode_switch_btn_event_cb, LV_EVENT_CLICKED, NULL);

    /* 内容区 */
    g_rgbled_ui.content_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(g_rgbled_ui.content_panel);
    lv_obj_set_size(g_rgbled_ui.content_panel, LV_PCT(100), LV_PCT(60));
    lv_obj_set_flex_flow(g_rgbled_ui.content_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_rgbled_ui.content_panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(g_rgbled_ui.content_panel, 5, 0);
    lv_obj_set_style_pad_gap(g_rgbled_ui.content_panel, 5, 0);
    lv_obj_clear_flag(g_rgbled_ui.content_panel, LV_OBJ_FLAG_SCROLLABLE);

    const char* slot_names[] = {"颜色 1", "颜色 2", "颜色 3"};
    for (uint8_t i = 0; i < 3; i++) {
        g_rgbled_ui.slot_panel[i] = lv_obj_create(g_rgbled_ui.content_panel);
        lv_obj_set_size(g_rgbled_ui.slot_panel[i], LV_PCT(31), LV_PCT(98));
        lv_obj_set_flex_flow(g_rgbled_ui.slot_panel[i], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(g_rgbled_ui.slot_panel[i], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(g_rgbled_ui.slot_panel[i], 8, 0);
        lv_obj_set_style_pad_gap(g_rgbled_ui.slot_panel[i], 20, 0);
        lv_obj_clear_flag(g_rgbled_ui.slot_panel[i], LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_add_flag(g_rgbled_ui.slot_panel[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_rgbled_ui.slot_panel[i], slot_panel_click_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(i + 1));

        g_rgbled_ui.slot_label[i] = lv_label_create(g_rgbled_ui.slot_panel[i]);
        lv_label_set_text(g_rgbled_ui.slot_label[i], slot_names[i]);
        lv_obj_set_style_text_font(g_rgbled_ui.slot_label[i], &my_font_yahei_24, 0);

        g_rgbled_ui.preview_led[i] = lv_led_create(g_rgbled_ui.slot_panel[i]);
        lv_obj_set_size(g_rgbled_ui.preview_led[i], 55, 55);
        lv_obj_set_style_border_width(g_rgbled_ui.preview_led[i], 3, 0);
        lv_obj_set_style_border_opa(g_rgbled_ui.preview_led[i], LV_OPA_100, 0);

        /* [NEW] 调整高光扩散范围（默认 LV_LED_BRIGHT_MAX = 2） */
        lv_obj_set_style_shadow_spread(g_rgbled_ui.preview_led[i], 8, LV_PART_MAIN);  // 范围：0-20，越大越扩散
        lv_obj_set_style_shadow_width(g_rgbled_ui.preview_led[i], 15, LV_PART_MAIN);  // 阴影宽度

        lv_obj_add_flag(g_rgbled_ui.preview_led[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_rgbled_ui.preview_led[i], led_click_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(i + 1));
        
        RGB_Color color;
        if (Drivers_RGBLED_GetSlotColor(i + 1, &color)) {
            lv_obj_set_style_bg_color(g_rgbled_ui.preview_led[i], lv_color_make(color.R, color.G, color.B), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(g_rgbled_ui.preview_led[i], LV_OPA_COVER, LV_PART_MAIN);
            set_led_border_smart(g_rgbled_ui.preview_led[i], color);
            lv_led_off(g_rgbled_ui.preview_led[i]);
        }

        lv_obj_t * separator = lv_obj_create(g_rgbled_ui.slot_panel[i]);
        lv_obj_remove_style_all(separator);
        lv_obj_set_size(separator, LV_PCT(80), 2);
        lv_obj_set_style_bg_color(separator, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_bg_opa(separator, LV_OPA_50, 0);

        create_rgb_slider_group(g_rgbled_ui.slot_panel[i], i);
    }

    /* 亮度面板 */
    g_rgbled_ui.brightness_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(g_rgbled_ui.brightness_panel);
    lv_obj_set_size(g_rgbled_ui.brightness_panel, LV_PCT(100), 50);
    lv_obj_set_flex_flow(g_rgbled_ui.brightness_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_rgbled_ui.brightness_panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(g_rgbled_ui.brightness_panel, 8, 0);

    lv_obj_t * brightness_icon = lv_label_create(g_rgbled_ui.brightness_panel);
    lv_label_set_text(brightness_icon, LV_SYMBOL_EYE_OPEN " 亮度调节: ");
    lv_obj_set_style_text_font(brightness_icon, &my_font_yahei_24, 0);

    g_rgbled_ui.brightness_slider = lv_slider_create(g_rgbled_ui.brightness_panel);
    lv_obj_set_size(g_rgbled_ui.brightness_slider, 320, 20);
    lv_slider_set_range(g_rgbled_ui.brightness_slider, 0, 255);
    lv_slider_set_value(g_rgbled_ui.brightness_slider, 255, LV_ANIM_OFF);
    lv_obj_add_event_cb(g_rgbled_ui.brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * brightness_value_label = lv_label_create(g_rgbled_ui.brightness_panel);
    lv_label_set_text(brightness_value_label, "255");
    lv_obj_set_style_text_font(brightness_value_label, &my_font_yahei_24, 0);
    lv_obj_set_width(brightness_value_label, 50);
    lv_obj_add_event_cb(g_rgbled_ui.brightness_slider, brightness_value_update_cb, LV_EVENT_VALUE_CHANGED, brightness_value_label);

    refresh_ui_for_mode();
    init_ui_from_driver_state();
}

/* -------------------- 对外接口 -------------------- */

void ui_screen_devices_details_init(lv_obj_t* parent, DeviceType_t device_type)
{
    /* 仅处理 RGB LED 类型，其他类型留空 */
    if (device_type == DEVICE_TYPE_RGBLED) {
        create_rgbled_details_ui(parent);
    }
}

void ui_screen_devices_details_deinit(void)
{
    memset(&g_rgbled_ui, 0, sizeof(rgbled_details_ui_t));
    if (g_header) {
        ui_comp_header_destroy(g_header);
        g_header = NULL;
    }
}

