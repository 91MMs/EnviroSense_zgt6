/**
 ******************************************************************************
 * @file    ui_screen_dashboard.c
 * @brief   主控台屏幕模块（整理版）
 * @details - 顶部：标准顶部栏组件（含系统时间）
 *          - 中部：1x4 数据显示栅格
 *          - 底部：1x3 设备控制栅格
 *          - 最底部：导航栏
 * @author  MmsY
 * @time    2025/11/23
 ******************************************************************************
 */

#include "ui_screen_dashboard.h"
#include "devices_manager.h"
#include "sensor_task.h"
#include "ui_comp_header.h"
#include "ui_comp_navbar.h"
#include "ui_manager.h"


/* 字体与资源声明 */
LV_FONT_DECLARE(my_font_yahei_24);
LV_IMG_DECLARE(led_symbol);
LV_IMG_DECLARE(beep_symbol);
LV_IMG_DECLARE(mygif);

/* UI 状态结构体 */
typedef struct {
  ui_header_t *header;

  /* 数据标签 */
  lv_obj_t *temp_label;
  lv_obj_t *humi_label;
  lv_obj_t *light_label;
  lv_obj_t *smoke_label;

  /* LED 控制 */
  lv_obj_t *led_indicator;
  lv_obj_t *led_cycle_btn;
  lv_obj_t *led_mode_btn;
  lv_obj_t *led_panel;

  /* 蜂鸣器 */
  lv_obj_t *beep_img;
  lv_anim_t beep_anim;
  lv_timer_t *beep_once_timer;

  /* GIF 动画 */
  lv_obj_t *gif_anim_obj;
  lv_anim_t gif_anim;

  /* 数据更新定时器 */
  lv_timer_t *data_update_timer;
} dashboard_ui_t;

static dashboard_ui_t g_ui;

/* 函数声明（按实现顺序） */
static void sync_led_controls_from_driver(void);
static void sensor_data_update_cb(lv_timer_t *timer);
static void data_panel_click_event_cb(lv_event_t *e);
static void led_cycle_btn_event_cb(lv_event_t *e);
static void led_mode_btn_event_cb(lv_event_t *e);
static void led_panel_click_event_cb(lv_event_t *e);
static void beep_switch_event_cb(lv_event_t *e);
static void beep_off_timer_cb(lv_timer_t *timer);
static void gif_switch_event_cb(lv_event_t *e);
static void set_angle_anim_cb(void *obj, int32_t v);
static void set_size_anim_cb(void *obj, int32_t v);
static void create_temp_humi_panel(lv_obj_t *parent, int grid_col);
static void create_single_data_panel(lv_obj_t *parent, int grid_col,
                                     const char *title, const char *unit,
                                     lv_obj_t **value_label,
                                     SensorType_t sensor_type);
static void create_led_panel(lv_obj_t *parent, int grid_col, int grid_row);
static void create_beep_panel(lv_obj_t *parent, int grid_col, int grid_row);
static void create_gif_panel(lv_obj_t *parent, int grid_col, int grid_row);

/* -------------------- 帮助函数 -------------------- */

/* 从驱动层同步 LED 控制区状态 */
static void sync_led_controls_from_driver(void) {
  led_control_mode_t mode = Drivers_RGBLED_GetMode();
  lv_obj_t *mode_label = lv_obj_get_child(g_ui.led_mode_btn, 0);

  if (mode == LED_MODE_AUTO) {
    lv_label_set_text(mode_label, LV_SYMBOL_REFRESH " 自动");
    lv_obj_add_state(g_ui.led_cycle_btn, LV_STATE_DISABLED);
  } else {
    lv_label_set_text(mode_label, LV_SYMBOL_SETTINGS " 手动");
    lv_obj_clear_state(g_ui.led_cycle_btn, LV_STATE_DISABLED);
  }

  if (mode == LED_MODE_MANUAL) {
    led_manual_state_t state = Drivers_RGBLED_GetManualState();
    RGB_Color slot_color;
    lv_obj_t *cycle_label = lv_obj_get_child(g_ui.led_cycle_btn, 0);

    switch (state) {
    case LED_STATE_SLOT_1:
      if (Drivers_RGBLED_GetSlotColor(1, &slot_color)) {
        lv_led_set_color(
            g_ui.led_indicator,
            lv_color_make(slot_color.R, slot_color.G, slot_color.B));
        lv_led_on(g_ui.led_indicator);
      }
      lv_label_set_text(cycle_label, "颜色1");
      break;

    case LED_STATE_SLOT_2:
      if (Drivers_RGBLED_GetSlotColor(2, &slot_color)) {
        lv_led_set_color(
            g_ui.led_indicator,
            lv_color_make(slot_color.R, slot_color.G, slot_color.B));
        lv_led_on(g_ui.led_indicator);
      }
      lv_label_set_text(cycle_label, "颜色2");
      break;

    case LED_STATE_SLOT_3:
      if (Drivers_RGBLED_GetSlotColor(3, &slot_color)) {
        lv_led_set_color(
            g_ui.led_indicator,
            lv_color_make(slot_color.R, slot_color.G, slot_color.B));
        lv_led_on(g_ui.led_indicator);
      }
      lv_label_set_text(cycle_label, "颜色3");
      break;

    case LED_STATE_OFF:
    default:
      lv_led_off(g_ui.led_indicator);
      lv_label_set_text(cycle_label, "关闭");
      break;
    }
  } else {
    RGB_Color current_color = Drivers_RGBLED_GetColor();
    if (current_color.R == 0 && current_color.G == 0 && current_color.B == 0) {
      lv_led_off(g_ui.led_indicator);
    } else {
      lv_led_set_color(
          g_ui.led_indicator,
          lv_color_make(current_color.R, current_color.G, current_color.B));
      lv_led_on(g_ui.led_indicator);
    }
  }
}

/* -------------------- 定时器回调 -------------------- */

/* 定时从 sensor_task 获取并更新 UI 数据 */
static void sensor_data_update_cb(lv_timer_t *timer) {
  SensorData_t data;

  /* 温湿度 */
  if (SensorTask_GetSensorData(SENSOR_TYPE_SHT30, &data) && data.is_valid) {
    if (g_ui.temp_label)
      lv_label_set_text_fmt(g_ui.temp_label, "%.1f", data.values.sht30.temp);
    if (g_ui.humi_label)
      lv_label_set_text_fmt(g_ui.humi_label, "%.1f", data.values.sht30.humi);
  } else {
    if (g_ui.temp_label)
      lv_label_set_text(g_ui.temp_label, "--.-");
    if (g_ui.humi_label)
      lv_label_set_text(g_ui.humi_label, "--.-");
  }

  /* 光照 */
  if (SensorTask_GetSensorData(SENSOR_TYPE_GY30, &data) && data.is_valid) {
    if (g_ui.light_label)
      lv_label_set_text_fmt(g_ui.light_label, "%d", (int)data.values.gy30.lux);

    /* 自动模式下根据光照调节 LED */
    if (Drivers_RGBLED_GetMode() == LED_MODE_AUTO) {
      Drivers_RGBLED_AutoAdjust(data.values.gy30.lux);
      RGB_Color current_color = Drivers_RGBLED_GetColor();
      if (current_color.R == 0 && current_color.G == 0 &&
          current_color.B == 0) {
        lv_led_off(g_ui.led_indicator);
      } else {
        lv_led_set_color(
            g_ui.led_indicator,
            lv_color_make(current_color.R, current_color.G, current_color.B));
        lv_led_on(g_ui.led_indicator);
      }
    }
  } else {
    if (g_ui.light_label)
      lv_label_set_text(g_ui.light_label, "--");
  }

  /* 烟感 */
  if (SensorTask_GetSensorData(SENSOR_TYPE_SMOKE, &data) && data.is_valid) {
    if (g_ui.smoke_label)
      lv_label_set_text_fmt(g_ui.smoke_label, "%d", data.values.smoke.ppm);
  } else {
    if (g_ui.smoke_label)
      lv_label_set_text(g_ui.smoke_label, "--");
  }
}

/* 蜂鸣器关闭定时器 */
static void beep_off_timer_cb(lv_timer_t *timer) {
  lv_anim_set_values(&g_ui.beep_anim, 900, 0);
  lv_anim_start(&g_ui.beep_anim);
}

/* -------------------- 事件回调 -------------------- */

/* 数据面板点击：跳转到传感器详情页 */
static void data_panel_click_event_cb(lv_event_t *e) {
  SensorType_t target_sensor =
      (SensorType_t)(intptr_t)lv_event_get_user_data(e);
  if (target_sensor != SENSOR_TYPE_NONE) {
    ui_set_active_sensor(target_sensor);
    ui_load_screen(UI_SCREEN_SENSORS_DETAILS);
  }
}

/* LED 手动循环按钮 */
static void led_cycle_btn_event_cb(lv_event_t *e) {
  Drivers_RGBLED_CycleColor();
  led_manual_state_t state = Drivers_RGBLED_GetManualState();
  RGB_Color slot_color;

  switch (state) {
  case LED_STATE_SLOT_1:
    if (Drivers_RGBLED_GetSlotColor(1, &slot_color)) {
      lv_led_set_color(g_ui.led_indicator,
                       lv_color_make(slot_color.R, slot_color.G, slot_color.B));
      lv_led_on(g_ui.led_indicator);
    }
    lv_label_set_text(lv_obj_get_child(g_ui.led_cycle_btn, 0), "颜色1");
    break;

  case LED_STATE_SLOT_2:
    if (Drivers_RGBLED_GetSlotColor(2, &slot_color)) {
      lv_led_set_color(g_ui.led_indicator,
                       lv_color_make(slot_color.R, slot_color.G, slot_color.B));
      lv_led_on(g_ui.led_indicator);
    }
    lv_label_set_text(lv_obj_get_child(g_ui.led_cycle_btn, 0), "颜色2");
    break;

  case LED_STATE_SLOT_3:
    if (Drivers_RGBLED_GetSlotColor(3, &slot_color)) {
      lv_led_set_color(g_ui.led_indicator,
                       lv_color_make(slot_color.R, slot_color.G, slot_color.B));
      lv_led_on(g_ui.led_indicator);
    }
    lv_label_set_text(lv_obj_get_child(g_ui.led_cycle_btn, 0), "颜色3");
    break;

  case LED_STATE_OFF:
  default:
    lv_led_off(g_ui.led_indicator);
    lv_label_set_text(lv_obj_get_child(g_ui.led_cycle_btn, 0), "关闭");
    break;
  }
}

/* LED 模式切换按钮 */
static void led_mode_btn_event_cb(lv_event_t *e) {
  led_control_mode_t current_mode = Drivers_RGBLED_GetMode();
  if (current_mode == LED_MODE_MANUAL) {
    Drivers_RGBLED_SetMode(LED_MODE_AUTO);
    lv_label_set_text(lv_obj_get_child(g_ui.led_mode_btn, 0),
                      LV_SYMBOL_REFRESH " 自动");
    lv_obj_add_state(g_ui.led_cycle_btn, LV_STATE_DISABLED);
  } else {
    Drivers_RGBLED_SetMode(LED_MODE_MANUAL);
    lv_label_set_text(lv_obj_get_child(g_ui.led_mode_btn, 0),
                      LV_SYMBOL_SETTINGS " 手动");
    lv_obj_clear_state(g_ui.led_cycle_btn, LV_STATE_DISABLED);
  }
}

/* LED 面板点击：进入详情页 */
static void led_panel_click_event_cb(lv_event_t *e) {
  ui_set_active_device(DEVICE_TYPE_RGBLED);
  ui_load_screen(UI_SCREEN_DEVICE_DETAILS);
}

/* 蜂鸣器开关 */
static void beep_switch_event_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
    lv_anim_set_values(&g_ui.beep_anim, 0, 900);
    lv_anim_start(&g_ui.beep_anim);
    if (g_ui.beep_once_timer)
      lv_timer_del(g_ui.beep_once_timer);
    g_ui.beep_once_timer = lv_timer_create(beep_off_timer_cb, 1500, NULL);
    lv_timer_set_repeat_count(g_ui.beep_once_timer, 1);
  } else {
    lv_anim_set_values(&g_ui.beep_anim, 900, 0);
    lv_anim_start(&g_ui.beep_anim);
  }
}

/* GIF 动画开关 */
static void gif_switch_event_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
    lv_anim_set_values(&g_ui.gif_anim, 40, 100);
    lv_anim_start(&g_ui.gif_anim);
  } else {
    lv_anim_set_values(&g_ui.gif_anim, 100, 40);
    lv_anim_start(&g_ui.gif_anim);
  }
}

/* 动画回调：旋转角度 */
static void set_angle_anim_cb(void *obj, int32_t v) {
  lv_img_set_angle(obj, v);
}

/* 动画回调：尺寸缩放 */
static void set_size_anim_cb(void *obj, int32_t v) {
  lv_obj_set_size(obj, v, v);
}

/* -------------------- UI 创建函数 -------------------- */

/* 创建温湿度组合面板 */
static void create_temp_humi_panel(lv_obj_t *parent, int grid_col) {
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_grid_cell(panel, LV_GRID_ALIGN_STRETCH, grid_col, 2,
                       LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(panel, data_panel_click_event_cb, LV_EVENT_CLICKED,
                      (void *)SENSOR_TYPE_SHT30);

  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *title_label = lv_label_create(panel);
  lv_label_set_text(title_label, "温湿度");
  lv_obj_set_style_text_font(title_label, &my_font_yahei_24, 0);

  lv_obj_t *data_container = lv_obj_create(panel);
  lv_obj_remove_style_all(data_container);
  lv_obj_set_size(data_container, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(data_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(data_container, LV_FLEX_ALIGN_SPACE_AROUND,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  /* 温度 */
  lv_obj_t *temp_container = lv_obj_create(data_container);
  lv_obj_remove_style_all(temp_container);
  lv_obj_set_size(temp_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(temp_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(temp_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  g_ui.temp_label = lv_label_create(temp_container);
  lv_label_set_text(g_ui.temp_label, "--.-");
  lv_obj_t *temp_unit = lv_label_create(temp_container);
  lv_label_set_text(temp_unit, "℃");

  /* 湿度 */
  lv_obj_t *humi_container = lv_obj_create(data_container);
  lv_obj_remove_style_all(humi_container);
  lv_obj_set_size(humi_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(humi_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(humi_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  g_ui.humi_label = lv_label_create(humi_container);
  lv_label_set_text(g_ui.humi_label, "--.-");
  lv_obj_t *humi_unit = lv_label_create(humi_container);
  lv_label_set_text(humi_unit, "%RH");

  lv_obj_set_style_text_font(g_ui.temp_label, &my_font_yahei_24, 0);
  lv_obj_set_style_text_font(temp_unit, &my_font_yahei_24, 0);
  lv_obj_set_style_text_font(g_ui.humi_label, &my_font_yahei_24, 0);
  lv_obj_set_style_text_font(humi_unit, &my_font_yahei_24, 0);
}

/* 创建单个数据面板 */
static void create_single_data_panel(lv_obj_t *parent, int grid_col,
                                     const char *title, const char *unit,
                                     lv_obj_t **value_label,
                                     SensorType_t sensor_type) {
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_grid_cell(panel, LV_GRID_ALIGN_STRETCH, grid_col, 1,
                       LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(panel, data_panel_click_event_cb, LV_EVENT_CLICKED,
                      (void *)sensor_type);

  lv_obj_t *title_label = lv_label_create(panel);
  lv_label_set_text(title_label, title);
  lv_obj_set_style_text_font(title_label, &my_font_yahei_24, 0);
  lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);

  lv_obj_t *value_container = lv_obj_create(panel);
  lv_obj_remove_style_all(value_container);
  lv_obj_set_size(value_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(value_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(value_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(value_container, 5, 0);
  lv_obj_align(value_container, LV_ALIGN_CENTER, 0, 10);

  *value_label = lv_label_create(value_container);
  lv_label_set_text(*value_label, "--");
  lv_obj_set_style_text_font(*value_label, &my_font_yahei_24, 0);

  lv_obj_t *unit_label = lv_label_create(value_container);
  lv_label_set_text(unit_label, unit);
  lv_obj_set_style_text_font(unit_label, &my_font_yahei_24, 0);
}

/* 创建 LED 控制面板 */
static void create_led_panel(lv_obj_t *parent, int grid_col, int grid_row) {
  g_ui.led_panel = lv_obj_create(parent);
  lv_obj_set_grid_cell(g_ui.led_panel, LV_GRID_ALIGN_STRETCH, grid_col, 1,
                       LV_GRID_ALIGN_STRETCH, grid_row, 1);
  lv_obj_set_style_pad_all(g_ui.led_panel, 15, 0);
  lv_obj_add_flag(g_ui.led_panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_ui.led_panel, led_panel_click_event_cb,
                      LV_EVENT_CLICKED, NULL);

  lv_obj_t *led_img = lv_img_create(g_ui.led_panel);
  lv_img_set_src(led_img, &led_symbol);
  lv_obj_align(led_img, LV_ALIGN_CENTER, 0, -25);

  g_ui.led_indicator = lv_led_create(g_ui.led_panel);
  lv_led_set_color(g_ui.led_indicator, lv_palette_main(LV_PALETTE_RED));
  lv_led_off(g_ui.led_indicator);
  lv_obj_set_size(g_ui.led_indicator, 30, 30);
  lv_obj_align(g_ui.led_indicator, LV_ALIGN_CENTER, 0, 0);

  lv_obj_set_style_border_color(g_ui.led_indicator, lv_color_black(), 0);
  lv_obj_set_style_border_width(g_ui.led_indicator, 2, 0); // 2px 边框
  lv_obj_set_style_border_opa(g_ui.led_indicator, LV_OPA_COVER,
                              0); // 完全不透明

  lv_obj_t *btn_container = lv_obj_create(g_ui.led_panel);
  lv_obj_remove_style_all(btn_container);
  lv_obj_set_size(btn_container, LV_PCT(90), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(btn_container, 8, 0);
  lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);

  g_ui.led_cycle_btn = lv_btn_create(btn_container);
  lv_obj_set_style_pad_all(g_ui.led_cycle_btn, 8, 0);
  lv_obj_t *cycle_label = lv_label_create(g_ui.led_cycle_btn);
  lv_label_set_text(cycle_label, "关闭");
  lv_obj_set_style_text_font(cycle_label, &my_font_yahei_24, 0);
  lv_obj_add_event_cb(g_ui.led_cycle_btn, led_cycle_btn_event_cb,
                      LV_EVENT_CLICKED, NULL);

  g_ui.led_mode_btn = lv_btn_create(btn_container);
  lv_obj_set_style_pad_all(g_ui.led_mode_btn, 8, 0);
  lv_obj_t *mode_label = lv_label_create(g_ui.led_mode_btn);
  lv_label_set_text(mode_label, LV_SYMBOL_SETTINGS " 手动");
  lv_obj_set_style_text_font(mode_label, &my_font_yahei_24, 0);
  lv_obj_add_event_cb(g_ui.led_mode_btn, led_mode_btn_event_cb,
                      LV_EVENT_CLICKED, NULL);
}

/* 创建蜂鸣器面板 */
static void create_beep_panel(lv_obj_t *parent, int grid_col, int grid_row) {
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_grid_cell(panel, LV_GRID_ALIGN_STRETCH, grid_col, 1,
                       LV_GRID_ALIGN_STRETCH, grid_row, 1);

  g_ui.beep_img = lv_img_create(panel);
  lv_img_set_src(g_ui.beep_img, &beep_symbol);
  lv_obj_align(g_ui.beep_img, LV_ALIGN_CENTER, 0, -10);
  lv_img_set_pivot(g_ui.beep_img, lv_obj_get_width(g_ui.beep_img) / 2,
                   lv_obj_get_height(g_ui.beep_img) / 2);

  lv_anim_init(&g_ui.beep_anim);
  lv_anim_set_var(&g_ui.beep_anim, g_ui.beep_img);
  lv_anim_set_exec_cb(&g_ui.beep_anim, set_angle_anim_cb);
  lv_anim_set_time(&g_ui.beep_anim, 500);

  lv_obj_t *beep_switch = lv_switch_create(panel);
  lv_obj_align(beep_switch, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_event_cb(beep_switch, beep_switch_event_cb, LV_EVENT_VALUE_CHANGED,
                      NULL);
}

/* 创建 GIF 动画面板 */
static void create_gif_panel(lv_obj_t *parent, int grid_col, int grid_row) {
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_grid_cell(panel, LV_GRID_ALIGN_STRETCH, grid_col, 1,
                       LV_GRID_ALIGN_STRETCH, grid_row, 1);

  g_ui.gif_anim_obj = lv_gif_create(panel);
  lv_gif_set_src(g_ui.gif_anim_obj, &mygif);
  lv_obj_set_size(g_ui.gif_anim_obj, 40, 40);
  lv_obj_align(g_ui.gif_anim_obj, LV_ALIGN_CENTER, 0, -10);

  lv_anim_init(&g_ui.gif_anim);
  lv_anim_set_var(&g_ui.gif_anim, g_ui.gif_anim_obj);
  lv_anim_set_exec_cb(&g_ui.gif_anim, set_size_anim_cb);
  lv_anim_set_time(&g_ui.gif_anim, 500);

  lv_obj_t *other_switch = lv_switch_create(panel);
  lv_obj_align(other_switch, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_event_cb(other_switch, gif_switch_event_cb, LV_EVENT_VALUE_CHANGED,
                      NULL);
}

/* -------------------- 对外接口 -------------------- */

void ui_screen_dashboard_init(lv_obj_t *parent) {
  memset(&g_ui, 0, sizeof(dashboard_ui_t));
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);

  /* 顶部栏 */
  ui_header_config_t header_config = {.title = "主页: 数据显示与控制",
                                      .show_back_btn = false,
                                      .show_custom_btn = false,
                                      .custom_btn_text = NULL,
                                      .back_btn_cb = NULL,
                                      .custom_btn_cb = NULL,
                                      .user_data = NULL,
                                      .show_time = true};
  g_ui.header = ui_comp_header_create(parent, &header_config);

  /* 主内容区 */
  lv_obj_t *content_panel = lv_obj_create(parent);
  lv_obj_remove_style_all(content_panel);
  lv_obj_set_flex_grow(content_panel, 1);
  lv_obj_set_width(content_panel, LV_PCT(100));
  lv_obj_set_flex_flow(content_panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(content_panel, 10, 0);
  lv_obj_set_style_pad_gap(content_panel, 10, 0);

  /* 数据显示区 */
  lv_obj_t *data_grid = lv_obj_create(content_panel);
  lv_obj_remove_style_all(data_grid);
  lv_obj_set_width(data_grid, LV_PCT(100));
  lv_obj_set_height(data_grid, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_gap(data_grid, 10, 0);

  static lv_coord_t data_col[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                  LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t data_row[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(data_grid, data_col, data_row);

  create_temp_humi_panel(data_grid, 0);
  create_single_data_panel(data_grid, 2, "光照", "Lux", &g_ui.light_label,
                           SENSOR_TYPE_GY30);
  create_single_data_panel(data_grid, 3, "烟感", "PPM", &g_ui.smoke_label,
                           SENSOR_TYPE_SMOKE);

  /* 设备控制区 */
  lv_obj_t *ctrl_grid = lv_obj_create(content_panel);
  lv_obj_remove_style_all(ctrl_grid);
  lv_obj_set_width(ctrl_grid, LV_PCT(100));
  lv_obj_set_flex_grow(ctrl_grid, 1);
  lv_obj_set_style_pad_gap(ctrl_grid, 10, 0);

  static lv_coord_t ctrl_col[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                  LV_GRID_TEMPLATE_LAST};
  static lv_coord_t ctrl_row[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(ctrl_grid, ctrl_col, ctrl_row);

  create_led_panel(ctrl_grid, 0, 0);
  create_beep_panel(ctrl_grid, 1, 0);
  create_gif_panel(ctrl_grid, 2, 0);

  /* 底部导航栏 */
  ui_comp_navbar_create(parent, UI_SCREEN_DASHBOARD);

  /* 同步 LED 状态 */
  sync_led_controls_from_driver();

  /* 启动数据更新定时器 */
  g_ui.data_update_timer = lv_timer_create(sensor_data_update_cb, 500, NULL);
  lv_timer_set_repeat_count(g_ui.data_update_timer, -1);
}

void ui_screen_dashboard_deinit(void) {
  if (g_ui.header) {
    ui_comp_header_destroy(g_ui.header);
    g_ui.header = NULL;
  }

  if (g_ui.data_update_timer) {
    lv_timer_del(g_ui.data_update_timer);
    g_ui.data_update_timer = NULL;
  }

  if (g_ui.beep_once_timer) {
    lv_timer_del(g_ui.beep_once_timer);
    g_ui.beep_once_timer = NULL;
  }
}
