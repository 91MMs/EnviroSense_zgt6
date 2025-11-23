/**
 ******************************************************************************
 * @file    ui_comp_navbar.c
 * @brief   底部导航栏组件
 * @details 提供主页、传感器列表、设备列表、设置四个导航按钮
 * @author  MmsY
 * @time    2025/11/23
 ******************************************************************************
 */

#include "ui_comp_navbar.h"
#include "ui_manager.h"

/* 导航栏高度 */
#define NAVBAR_HEIGHT 70

/* 导航栏背景色 */
#define NAVBAR_BG_COLOR 0xF5EFE6

/**
 * @brief 导航按钮点击事件回调
 */
static void nav_button_event_cb(lv_event_t *e) {
  ui_screen_t target_screen = (ui_screen_t)(intptr_t)lv_event_get_user_data(e);
  ui_load_screen(target_screen);
}

/**
 * @brief 创建底部导航栏
 */
lv_obj_t *ui_comp_navbar_create(lv_obj_t *parent, ui_screen_t active_screen) {
  /* === 1. 创建导航栏容器 === */
  lv_obj_t *nav_bar = lv_obj_create(parent);
  lv_obj_remove_style_all(nav_bar);
  lv_obj_set_size(nav_bar, LV_PCT(100), NAVBAR_HEIGHT);
  lv_obj_set_style_bg_color(nav_bar, lv_color_hex(NAVBAR_BG_COLOR), 0);
  lv_obj_set_style_bg_opa(nav_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(nav_bar, 0, 0);
  lv_obj_set_style_border_width(nav_bar, 0, 0);

  /* === 2. 创建按钮容器（水平 Flex 布局） === */
  lv_obj_t *btn_container = lv_obj_create(nav_bar);
  lv_obj_remove_style_all(btn_container);
  lv_obj_set_size(btn_container, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_container,
                        LV_FLEX_ALIGN_SPACE_EVENLY, /* 均匀分布 */
                        LV_FLEX_ALIGN_CENTER,       /* 垂直居中 */
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_center(btn_container);
  lv_obj_set_style_pad_hor(btn_container, 10, 0);
  lv_obj_set_style_pad_gap(btn_container, 10, 0);

  /* === 3. 定义导航按钮配置 === */
  typedef struct {
    const char *symbol;        /* 按钮图标 */
    lv_color_t color;          /* 按钮颜色 */
    ui_screen_t target_screen; /* 目标屏幕 */
  } nav_button_config_t;

  const nav_button_config_t buttons[] = {
      /* 主页按钮 */
      {.symbol = LV_SYMBOL_HOME,
       .color = lv_palette_main(LV_PALETTE_BLUE),
       .target_screen = UI_SCREEN_DASHBOARD},
      /* 传感器列表按钮 */
      {
          .symbol = LV_SYMBOL_LIST,
          .color = lv_palette_main(LV_PALETTE_GREEN),
          .target_screen = UI_SCREEN_SENSORS_LISTS // [CHANGED] 使用新枚举名
      },
      /* 设备列表按钮（可选） */
      {
          .symbol = LV_SYMBOL_SETTINGS, // 可改为设备图标
          .color = lv_palette_main(LV_PALETTE_ORANGE),
          .target_screen =
              UI_SCREEN_DEVICE_DETAILS // 或创建 UI_SCREEN_DEVICES_LISTS
      },
      /* 设置/登出按钮 */
      {.symbol = LV_SYMBOL_POWER,
       .color = lv_palette_main(LV_PALETTE_RED),
       .target_screen = UI_SCREEN_LOGIN}};

  const uint8_t button_count = sizeof(buttons) / sizeof(buttons[0]);

  /* === 4. 循环创建导航按钮 === */
  for (uint8_t i = 0; i < button_count; i++) {
    lv_obj_t *btn = lv_btn_create(btn_container);
    lv_obj_set_flex_grow(btn, 1); /* 按钮等分剩余空间 */
    lv_obj_set_style_bg_color(btn, buttons[i].color, 0);
    lv_obj_set_style_radius(btn, 8, 0); /* 圆角 */

    /* 创建按钮图标 */
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, buttons[i].symbol);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_center(label);

    /* 添加点击事件 */
    lv_obj_add_event_cb(btn, nav_button_event_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)buttons[i].target_screen);

    /* === 高亮当前活动按钮 === */
    if (active_screen == buttons[i].target_screen) {
      lv_obj_set_style_outline_width(btn, 3, 0);
      lv_obj_set_style_outline_color(btn, lv_color_white(), 0);
      lv_obj_set_style_outline_pad(btn, 3, 0);
      lv_obj_set_style_shadow_width(btn, 10, 0); /* 添加阴影增强效果 */
      lv_obj_set_style_shadow_color(btn, buttons[i].color, 0);
    }
  }

  return nav_bar;
}
