/**
 ******************************************************************************
 * @file    ui_manager.c
 * @brief   UI中央屏幕管理器
 * @details 负责所有屏幕的创建、销毁、切换和上下文传递。
 * @author  MmsY
 * @time    2025/11/23
 ******************************************************************************
 */

#include "ui_manager.h"
#include "lvgl.h"

/* 引入所有屏幕模块的头文件 */
#include "ui_screen_boot.h"
#include "ui_screen_dashboard.h"
#include "ui_screen_devices_details.h"
#include "ui_screen_login.h"
#include "ui_screen_sensors_details.h" // [CHANGED] 新文件名
#include "ui_screen_sensors_lists.h"   // [CHANGED] 新文件名


/* 私有全局变量 */
static lv_obj_t *g_current_screen_container = NULL;
static ui_screen_t g_current_screen_id = UI_SCREEN_NONE;
static ui_screen_t g_previous_screen_id = UI_SCREEN_NONE;
static SensorType_t g_active_sensor_for_details = SENSOR_TYPE_NONE;
static DeviceType_t g_active_device_type = DEVICE_TYPE_RGBLED;

/* -----------------------------------------------------------
 * 公共函数
 * ----------------------------------------------------------- */

/**
 * @brief 设置要显示的传感器类型
 */
void ui_set_active_sensor(SensorType_t type) {
  g_active_sensor_for_details = type;
}

/**
 * @brief 获取当前活动传感器类型
 */
SensorType_t ui_get_active_sensor(void) { return g_active_sensor_for_details; }

/**
 * @brief 设置当前活动设备类型
 */
void ui_set_active_device(DeviceType_t type) { g_active_device_type = type; }

/**
 * @brief 获取当前活动设备类型
 */
DeviceType_t ui_get_active_device(void) { return g_active_device_type; }

/**
 * @brief 加载指定屏幕
 */
void ui_load_screen(ui_screen_t screen) {
  if (screen != g_current_screen_id) {
    g_previous_screen_id = g_current_screen_id;
  }

  if (screen == g_current_screen_id) {
    return;
  }

  /* 获取当前活动屏幕 */
  lv_obj_t *active_scr = lv_scr_act();

  /* 在销毁旧屏幕之前，调用其专属的清理函数 */
  switch (g_current_screen_id) {
  case UI_SCREEN_DASHBOARD:
    ui_screen_dashboard_deinit();
    break;
  case UI_SCREEN_SENSORS_DETAILS:       // [CHANGED] 新枚举名
    ui_screen_sensors_details_deinit(); // [CHANGED] 新函数名
    break;
  case UI_SCREEN_SENSORS_LISTS:       // [CHANGED] 新枚举名
    ui_screen_sensors_lists_deinit(); // [CHANGED] 新函数名
    break;
  case UI_SCREEN_DEVICE_DETAILS:
    ui_screen_devices_details_deinit();
    break;
  default:
    break;
  }

  /* 异步删除旧容器 */
  if (g_current_screen_container) {
    lv_obj_del_async(g_current_screen_container);
    g_current_screen_container = NULL;
  }

  /* 为新屏幕创建一个根容器 */
  g_current_screen_container = lv_obj_create(active_scr);
  lv_obj_remove_style_all(g_current_screen_container);
  lv_obj_set_size(g_current_screen_container, LV_PCT(100), LV_PCT(100));

  /* 根据枚举加载新屏幕的UI */
  switch (screen) {
  case UI_SCREEN_BOOT:
    ui_screen_boot_init(g_current_screen_container);
    break;
  case UI_SCREEN_LOGIN:
    ui_screen_login_init(g_current_screen_container);
    break;
  case UI_SCREEN_DASHBOARD:
    ui_screen_dashboard_init(g_current_screen_container);
    break;
  case UI_SCREEN_SENSORS_DETAILS: // [CHANGED] 新枚举名
    ui_screen_sensors_details_init(
        g_current_screen_container); // [CHANGED] 新函数名
    break;
  case UI_SCREEN_SENSORS_LISTS: // [CHANGED] 新枚举名
    ui_screen_sensors_lists_init(
        g_current_screen_container); // [CHANGED] 新函数名
    break;
  case UI_SCREEN_DEVICE_DETAILS:
    ui_screen_devices_details_init(g_current_screen_container,
                                   g_active_device_type);
    break;
  case UI_SCREEN_SETTINGS:
    // ui_screen_settings_init(g_current_screen_container);
    break;
  default:
    break;
  }

  g_current_screen_id = screen;
}

/**
 * @brief 初始化UI系统
 */
void ui_init(void) {
  // ui_load_screen(UI_SCREEN_BOOT);  // 开机动画
  ui_load_screen(UI_SCREEN_DASHBOARD); // 调试时直接加载主页
}

/**
 * @brief 返回上一个屏幕
 */
void ui_load_previous_screen(void) {
  if (g_previous_screen_id != UI_SCREEN_NONE) {
    ui_load_screen(g_previous_screen_id);
  } else {
    ui_load_screen(UI_SCREEN_DASHBOARD); // 默认返回主页
  }
}
