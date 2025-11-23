/**
 ******************************************************************************
 * @file    ui_screen_boot.c
 * @brief   开机动画屏幕模块
 * @details 负责显示开机时的打字效果和作者信息动画。
 *          动画结束后，通过ui_manager加载登录屏幕。
 * @author  MmsY
 * @time    2025/11/23
 ******************************************************************************
 */

#include "ui_screen_boot.h"
#include "string.h"
#include "ui_manager.h" // [CHANGED] 引入UI管理器
#include <stdio.h>
#include <stdlib.h>


// --- LVGL资源声明 ---
LV_IMG_DECLARE(author_photo);
LV_IMG_DECLARE(bilbil);
LV_FONT_DECLARE(my_font_yahei_36);
LV_FONT_DECLARE(my_font_yahei_24);

/* --------------------- 模块私有定义 ------------------------- */

// 将所有静态全局变量封装到一个结构体中
typedef struct {
  lv_obj_t *author_obj;       // 作者信息的容器对象
  lv_obj_t *bilbil_img;       // B站logo图片对象
  lv_obj_t *author_photo_img; // 作者头像图片对象
  lv_obj_t *label_obj;        // 标签容器对象
  lv_obj_t *text_obj;         // 第一行文本容器
  lv_obj_t *text_obj1;        // 第二行文本容器
  lv_obj_t *text_label;       // 第一行打字效果文本标签
  lv_obj_t *text_label1;      // 第二行打字效果文本标签
  lv_timer_t *typing_timer;   // 打字效果定时器
} boot_screen_ui_t;

static boot_screen_ui_t g_ui; // 使用一个全局的结构体实例

// --- 私有函数前置声明 ---
static void create_typing_effect(lv_obj_t *parent);
static void lv_typing_effect(lv_timer_t *task);
static void text_anim_b_end(lv_anim_t *anim);
static void lv_boot_anim2_author(void);
static void obj_anim_end(lv_anim_t *var);
static void bounce_anim(void *var);
static void display_anim(void *var);
static void left_move_anim(void *var);
static void obj_hide_anim(void *var);

// --- 动画回调函数 ---
// 通用的透明度设置回调
static void anim_set_opa_cb(void *var, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

// B站图标弹跳动画的Y轴位置设置回调
static void bilbil_anim_y_cb(void *var, int32_t v) { lv_obj_set_y(var, v); }

// 作者照片显示动画的透明度设置回调
static void author_photo_anim_disp_cb(void *var, int32_t v) {
  lv_obj_set_style_img_opa(var, v, 0);
}

// 文字标签宽度变化回调（用于左移动画）
static void label_anim_y_cb(void *var, int32_t v) { lv_obj_set_width(var, v); }

// 元素隐藏动画的透明度设置回调
static void obj_hide_anim_cb(void *var, int32_t v) {
  lv_obj_set_style_opa(var, v, 0);
}

// --- 动画结束处理 ---

// B站图标弹跳动画结束后，启动照片和文字动画
static void bilbil_ovenr_anim_end(lv_anim_t *var) {
  display_anim(g_ui.author_photo_img);
  left_move_anim(g_ui.label_obj);
}

// 照片显示动画结束后，启动所有元素的隐藏动画
static void author_photo_anim_end(lv_anim_t *var) {
  obj_hide_anim(g_ui.bilbil_img);
  obj_hide_anim(g_ui.author_photo_img);
  obj_hide_anim(g_ui.label_obj);
}

// [关键修改] 所有动画的最终回调：通知UI管理器切换到登录界面
static void obj_anim_end(lv_anim_t *var) {
  // 动画流程结束，加载下一个屏幕
  ui_load_screen(UI_SCREEN_LOGIN);
}

// [关键修改] 打字效果的渐隐动画结束后，启动第二阶段的作者动画
static void text_anim_b_end(lv_anim_t *anim) { lv_boot_anim2_author(); }

// --- 动画流程启动函数 ---
// B站图标的弹跳动画
static void bounce_anim(void *var) {
  lv_anim_t a;
  lv_anim_init(&a);                                // 初始化动画描述符
  lv_anim_set_var(&a, var);                        // 设置动画目标
  lv_anim_set_values(&a, lv_obj_get_y(var), 100);  // 设置起始和结束Y坐标
  lv_anim_set_time(&a, 1500);                      // 设置动画时长(ms)
  lv_anim_set_exec_cb(&a, bilbil_anim_y_cb);       // 设置动画执行回调
  lv_anim_set_path_cb(&a, lv_anim_path_bounce);    // 设置弹跳路径
  lv_anim_set_ready_cb(&a, bilbil_ovenr_anim_end); // 设置完成回调
  lv_anim_start(&a);                               // 启动动画
}

// 作者头像的渐显效果的动画
static void display_anim(void *var) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, var);
  lv_anim_set_values(&a, 0, 255); // 设置透明度范围(0-255)
  lv_anim_set_time(&a, 2550);
  lv_anim_set_exec_cb(&a, author_photo_anim_disp_cb);
  lv_anim_set_ready_cb(&a, author_photo_anim_end);
  lv_anim_start(&a);
}

// 作者信息标签的左移动画
static void left_move_anim(void *var) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, var);
  lv_anim_set_values(&a, 0, 235); // 设置宽度范围
  lv_anim_set_time(&a, 2350);
  lv_anim_set_exec_cb(&a, label_anim_y_cb);
  lv_anim_set_path_cb(&a, lv_anim_path_overshoot); // 设置回弹路径
  lv_anim_start(&a);
}

// UI元素的隐藏动画
static void obj_hide_anim(void *var) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, var);
  lv_anim_set_values(&a, 255, 0);
  lv_anim_set_time(&a, 1500); // 调整隐藏时间
  lv_anim_set_delay(&a, 500); // 稍作延迟后开始隐藏
  lv_anim_set_exec_cb(&a, obj_hide_anim_cb);
  lv_anim_set_ready_cb(&a, obj_anim_end); // 所有隐藏动画结束后调用
  lv_anim_start(&a);
}

// --- UI创建函数 ---

// [CRITICAL FIX & REFACTORED] 打字效果定时器回调，已修复内存泄漏
static void lv_typing_effect(lv_timer_t *timer) {
  static const char *full_text = "STM32 ZGT6";
  static const char *full_text1 = "HAL库+FreeRTOS+LVGL";
  static size_t text_len = 0;
  static size_t text_len1 = 0;
  static char typing_buffer[128]; // 使用静态缓冲区，避免malloc

  if (text_len < strlen(full_text)) {
    text_len++;
    strncpy(typing_buffer, full_text, text_len);
    typing_buffer[text_len] = '\0';
    lv_label_set_text(g_ui.text_label, typing_buffer);
  } else if (text_len1 < strlen(full_text1)) {
    text_len1++;
    strncpy(typing_buffer, full_text1, text_len1);
    typing_buffer[text_len1] = '\0';
    lv_label_set_text(g_ui.text_label1, typing_buffer);
  } else {
    lv_timer_del(timer); // 打字效果完成后删除定时器
    g_ui.typing_timer = NULL;
    text_len = 0;
    text_len1 = 0;

    // 启动文字渐隐动画
    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, g_ui.text_label);
    lv_anim_set_values(&a1, 255, 0);
    lv_anim_set_exec_cb(&a1, anim_set_opa_cb);
    lv_anim_set_time(&a1, 1000);
    lv_anim_start(&a1);

    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, g_ui.text_label1);
    lv_anim_set_values(&a2, 255, 0);
    lv_anim_set_exec_cb(&a2, anim_set_opa_cb);
    lv_anim_set_time(&a2, 1000);
    lv_anim_set_ready_cb(&a2, text_anim_b_end); // 最后一个动画结束时调用
    lv_anim_start(&a2);
  }
}

// 创建第一阶段的打字效果UI
static void create_typing_effect(lv_obj_t *parent) {
  // 创建文本容器和标签
  g_ui.text_obj = lv_obj_create(parent);
  lv_obj_remove_style_all(g_ui.text_obj);
  lv_obj_set_size(g_ui.text_obj, 400, 100);
  lv_obj_align(g_ui.text_obj, LV_ALIGN_CENTER, 0, -50);

  g_ui.text_obj1 = lv_obj_create(parent);
  lv_obj_remove_style_all(g_ui.text_obj1);
  lv_obj_set_size(g_ui.text_obj1, 700, 100);
  lv_obj_align(g_ui.text_obj1, LV_ALIGN_CENTER, 0, 50);

  g_ui.text_label = lv_label_create(g_ui.text_obj);
  lv_obj_set_style_text_font(g_ui.text_label, &my_font_yahei_36, 0);
  lv_obj_set_style_text_color(g_ui.text_label, lv_color_white(), 0);
  lv_obj_center(g_ui.text_label);

  g_ui.text_label1 = lv_label_create(g_ui.text_obj1);
  lv_obj_set_style_text_font(g_ui.text_label1, &my_font_yahei_36, 0);
  lv_obj_set_style_text_color(g_ui.text_label1, lv_color_white(), 0);
  lv_obj_center(g_ui.text_label1);

  // 创建后清空
  lv_label_set_text(g_ui.text_label, "");
  lv_label_set_text(g_ui.text_label1, "");

  // 创建定时器，启动打字效果
  g_ui.typing_timer = lv_timer_create(lv_typing_effect, 300, NULL);
}

// 创建第二阶段的作者信息UI
static void lv_boot_anim2_author(void) {
  lv_obj_t *parent = lv_scr_act(); // 此时父对象就是屏幕本身

  g_ui.author_obj = lv_obj_create(parent);
  lv_obj_remove_style_all(g_ui.author_obj);
  lv_obj_set_size(g_ui.author_obj, 600, 480);
  lv_obj_center(g_ui.author_obj);

  g_ui.bilbil_img = lv_img_create(g_ui.author_obj);
  lv_img_set_src(g_ui.bilbil_img, &bilbil);
  lv_obj_align(g_ui.bilbil_img, LV_ALIGN_TOP_MID, 0, 0);

  g_ui.author_photo_img = lv_img_create(g_ui.author_obj);
  lv_img_set_src(g_ui.author_photo_img, &author_photo);
  lv_obj_align(g_ui.author_photo_img, LV_ALIGN_CENTER, -120, 50);
  lv_obj_set_style_img_opa(g_ui.author_photo_img, 0, 0);

  g_ui.label_obj = lv_obj_create(g_ui.author_obj);
  lv_obj_remove_style_all(g_ui.label_obj);
  lv_obj_set_size(g_ui.label_obj, 0, 110);
  lv_obj_align(g_ui.label_obj, LV_ALIGN_RIGHT_MID, -100, 50);

  lv_obj_t *name_label = lv_label_create(g_ui.label_obj);
  lv_label_set_text(name_label, "UP主：");
  lv_obj_set_style_text_font(name_label, &my_font_yahei_24, 0);
  lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFD700), 0);
  lv_obj_align(name_label, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *name_label1 = lv_label_create(g_ui.label_obj);
  lv_label_set_text(name_label1, "木木三鸭MmsY");
  lv_obj_set_style_text_font(name_label1, &my_font_yahei_24, 0);
  lv_obj_set_style_text_color(name_label1, lv_color_hex(0xFFD700), 0);
  lv_obj_align(name_label1, LV_ALIGN_BOTTOM_MID, -5, 0);

  // 启动第二阶段动画
  bounce_anim(g_ui.bilbil_img);
}

// --- 公共函数实现 ---

/**
 * @brief 初始化开机动画屏幕
 * @param parent 父对象 (通常是屏幕的根容器)
 */
void ui_screen_boot_init(lv_obj_t *parent) {
  // 清空全局结构体，确保没有残留数据
  memset(&g_ui, 0, sizeof(boot_screen_ui_t));

  // 设置父容器(即屏幕)的背景色
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

  // 启动第一阶段动画：打字效果
  create_typing_effect(parent);
}
