/**
 * @file      ui_screen_login.c
 * @brief     LVGL 登录屏幕模块 (最终版)
 * @details   本文件实现了登录界面的UI创建和事件处理逻辑。
 * - 使用结构体统一管理UI控件。
 * - 通过模态覆盖层优化了键盘和消息框的交互体验。
 * - 实现了密码框内容的“显示/隐藏”功能。
 * - 通过抢占事件处理，解决了复合控件的事件冲突问题。
 * @author    MmsY (Refactored and Finalized by Gemini)
 * @date      2025-09-28
 * @version   1.0
 */

/*********************************************************************************
 * 包含头文件
 *********************************************************************************/
#include "ui_screen_login.h"
#include "ui_manager.h" // 用于屏幕切换
#include <string.h>     // 用于 strcmp

/*********************************************************************************
 * 宏定义与全局变量
 *********************************************************************************/
// --- LVGL字体资源声明 (由外部工具生成) ---
LV_FONT_DECLARE(my_font_yahei_18);
LV_FONT_DECLARE(my_font_yahei_24);

/**
 * @brief 登录屏幕所有UI控件的句柄集合
 */
typedef struct {
    lv_obj_t * window_obj;         /**< 主功能窗口容器 */
    lv_obj_t * user_name_input;    /**< 用户名输入框 */
    lv_obj_t * password_input;     /**< 密码输入框 */
    lv_obj_t * password_eye_icon;  /**< “显示/隐藏密码”图标 */
    lv_obj_t * user_name_keyboard; /**< 用户名输入键盘 */
    lv_obj_t * password_keyboard;  /**< 密码输入键盘 */
    lv_obj_t * msgbox;             /**< 登录失败消息框 */
    lv_obj_t * msgbox_overlay;     /**< 消息框的模态覆盖层 */
    lv_obj_t * keyboard_overlay;   /**< 键盘的模态覆盖层 */
} login_screen_ui_t;

static login_screen_ui_t g_ui; /**< 本模块的UI控件全局实例 */

/*********************************************************************************
 * 内部函数前置声明
 *********************************************************************************/
static void create_login_widgets(lv_obj_t* parent);
static void create_message_box(lv_obj_t* parent);

// --- 事件回调函数 ---
static void textarea_focus_event_cb(lv_event_t * e);
static void password_focus_event_cb(lv_event_t * e);
static void login_btn_click_event_cb(lv_event_t* e);
static void password_eye_click_event_cb(lv_event_t * e);
static void keyboard_event_cb(lv_event_t * e);
static void keyboard_overlay_click_event_cb(lv_event_t * e);
static void msgbox_close_event_cb(lv_event_t * e);

// --- 统一逻辑处理函数 ---
static void hide_msgbox_and_overlay(void);
static void dismiss_all_keyboards(void);

/*********************************************************************************
 * 统一逻辑处理函数
 *********************************************************************************/

/**
 * @brief 统一的消息框关闭逻辑。
 * @details 隐藏消息框、其覆盖层，并确保键盘及覆盖层也被隐藏，同时清除输入框焦点。
 * 这是处理UI状态重置的核心函数。
 */
static void hide_msgbox_and_overlay(void) {
    if (g_ui.msgbox) lv_obj_add_flag(g_ui.msgbox, LV_OBJ_FLAG_HIDDEN);
    if (g_ui.msgbox_overlay) lv_obj_add_flag(g_ui.msgbox_overlay, LV_OBJ_FLAG_HIDDEN);
    
    // 清空输入框
    if (g_ui.user_name_input)   lv_textarea_set_text(g_ui.user_name_input, "");
    if (g_ui.password_input)    lv_textarea_set_text(g_ui.password_input, "");
    
    dismiss_all_keyboards(); // 复用键盘关闭逻辑，确保所有状态被重置
}

/**
 * @brief 统一的键盘关闭逻辑。
 * @details 隐藏两个键盘和键盘覆盖层，并清除两个输入框的焦点状态。
 */
static void dismiss_all_keyboards(void) {
    if (g_ui.user_name_keyboard) lv_obj_add_flag(g_ui.user_name_keyboard, LV_OBJ_FLAG_HIDDEN);
    if (g_ui.password_keyboard) lv_obj_add_flag(g_ui.password_keyboard, LV_OBJ_FLAG_HIDDEN);
    if (g_ui.keyboard_overlay) lv_obj_add_flag(g_ui.keyboard_overlay, LV_OBJ_FLAG_HIDDEN);
    if (g_ui.user_name_input) lv_obj_clear_state(g_ui.user_name_input, LV_STATE_FOCUSED);
    if (g_ui.password_input) lv_obj_clear_state(g_ui.password_input, LV_STATE_FOCUSED);
}

/*********************************************************************************
 * 事件回调函数定义
 *********************************************************************************/

/**
 * @brief 用户名输入框获得焦点时的回调函数。
 * @details 激活并显示用户名键盘及其覆盖层，同时隐藏密码键盘。
 * @param e 事件参数
 */
static void textarea_focus_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_FOCUSED) {
        lv_obj_clear_flag(g_ui.keyboard_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_ui.keyboard_overlay);

        lv_keyboard_set_textarea(g_ui.user_name_keyboard, g_ui.user_name_input);
        lv_obj_clear_flag(g_ui.user_name_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ui.password_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_ui.user_name_keyboard);
    }
}

/**
 * @brief 密码输入框获得焦点时的回调函数。
 * @details 激活并显示密码键盘及其覆盖层，同时隐藏用户名键盘。
 * @param e 事件参数
 */
static void password_focus_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_FOCUSED) {
        lv_obj_clear_flag(g_ui.keyboard_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_ui.keyboard_overlay);

        lv_keyboard_set_textarea(g_ui.password_keyboard, g_ui.password_input);
        lv_obj_clear_flag(g_ui.password_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ui.user_name_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_ui.password_keyboard);
    }
}

/**
 * @brief 虚拟键盘自带的“完成”或“取消”按钮的事件回调。
 * @param e 事件参数
 */
static void keyboard_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
       dismiss_all_keyboards();
    }
}

/**
 * @brief 键盘模态覆盖层的点击事件回调。
 * @param e 事件参数
 */
static void keyboard_overlay_click_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        dismiss_all_keyboards();
    }
}

/**
 * @brief 用于处理所有消息框关闭操作的通用回调函数。
 * @details 无论是点击覆盖层，还是点击消息框自带的关闭按钮，都会触发此函数。
 * @param e 事件参数
 */
static void msgbox_close_event_cb(lv_event_t * e) {
    hide_msgbox_and_overlay();
    // 阻止事件继续传播，防止LVGL的默认处理函数销毁消息框对象
    lv_event_stop_processing(e);
}

/**
 * @brief 登录按钮的点击事件回调。
 * @param e 事件参数
 */
static void login_btn_click_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        dismiss_all_keyboards(); // 首先收起键盘
        
        const char *user_txt = lv_textarea_get_text(g_ui.user_name_input);
        const char *password_txt = lv_textarea_get_text(g_ui.password_input);

        // 验证用户名和密码
        if((strcmp(user_txt, "stm32") == 0) && (strcmp(password_txt, "123456") == 0)) {
            ui_load_screen(UI_SCREEN_DASHBOARD);
        } else {
            // 显示错误提示
            if (g_ui.msgbox_overlay) lv_obj_clear_flag(g_ui.msgbox_overlay, LV_OBJ_FLAG_HIDDEN);
            if (g_ui.msgbox) {
                lv_obj_clear_flag(g_ui.msgbox, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(g_ui.msgbox);
            }
        }
    }
}

/**
 * @brief “显示/隐藏密码”图标的点击事件回调。
 * @param e 事件参数
 */
static void password_eye_click_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        bool is_password_mode = lv_textarea_get_password_mode(g_ui.password_input);
        lv_textarea_set_password_mode(g_ui.password_input, !is_password_mode);
        lv_label_set_text(g_ui.password_eye_icon, is_password_mode ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    }
}

/*********************************************************************************
 * UI 创建函数
 *********************************************************************************/

/**
 * @brief 创建登录失败时弹出的消息框及其覆盖层。
 * @param parent 消息框的父对象
 */
static void create_message_box(lv_obj_t* parent) {
    // 1. 创建模态覆盖层
    g_ui.msgbox_overlay = lv_obj_create(parent);
    lv_obj_set_size(g_ui.msgbox_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(g_ui.msgbox_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_ui.msgbox_overlay, LV_OPA_30, 0);
    lv_obj_add_flag(g_ui.msgbox_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_ui.msgbox_overlay, msgbox_close_event_cb, LV_EVENT_CLICKED, NULL);

    // 2. 创建消息框本体
    g_ui.msgbox = lv_msgbox_create(parent, "登录失败", "账号或密码错误, 请重试!", NULL, true);
    lv_obj_set_style_text_font(g_ui.msgbox, &my_font_yahei_18, 0);
    lv_obj_set_size(g_ui.msgbox, 280, 150);
    lv_obj_center(g_ui.msgbox);
    lv_obj_add_flag(g_ui.msgbox, LV_OBJ_FLAG_HIDDEN);
    
    // 3. 拦截自带关闭按钮的事件，确保我们的逻辑优先执行
    lv_obj_t* close_btn = lv_msgbox_get_close_btn(g_ui.msgbox);
    if (close_btn) {
        lv_obj_add_event_cb(close_btn, msgbox_close_event_cb, LV_EVENT_CLICKED | LV_EVENT_PREPROCESS, NULL);
    }
}

/**
 * @brief 创建登录界面的所有核心控件。
 * @param parent 控件的父对象
 */
static void create_login_widgets(lv_obj_t* parent) {
    // 1. 创建主窗口容器
    g_ui.window_obj = lv_obj_create(parent);
    lv_obj_set_size(g_ui.window_obj, 400, 320);
    lv_obj_set_style_bg_opa(g_ui.window_obj, LV_OPA_30, 0);
    lv_obj_set_style_border_width(g_ui.window_obj, 0, 0);
    lv_obj_set_style_radius(g_ui.window_obj, 15, 0);
    lv_obj_center(g_ui.window_obj);
    
    // 2. 创建用户名输入行 (使用Flexbox布局)
    lv_obj_t* user_line = lv_obj_create(g_ui.window_obj);
    lv_obj_set_size(user_line, lv_pct(85), 48);
    lv_obj_align(user_line, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_flex_flow(user_line, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(user_line, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(user_line, 1, 0);
    lv_obj_set_style_border_color(user_line, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(user_line, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(user_line, 10, 0);
    lv_obj_set_style_pad_hor(user_line, 10, 0);
    lv_obj_set_style_pad_column(user_line, 5, 0);
    lv_obj_set_scrollbar_mode(user_line, LV_SCROLLBAR_MODE_OFF);

    // 2.1 用户图标
    lv_obj_t* user_icon = lv_label_create(user_line);
    lv_obj_set_style_text_font(user_icon, &lv_font_montserrat_20, 0);
    lv_label_set_text(user_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(user_icon, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);

    // 2.2 用户名输入框
    g_ui.user_name_input = lv_textarea_create(user_line);
    lv_obj_set_style_text_font(g_ui.user_name_input, &lv_font_montserrat_20, 0);
    lv_textarea_set_placeholder_text(g_ui.user_name_input, "name (stm32)");
    lv_obj_set_flex_grow(g_ui.user_name_input, 1);
    lv_textarea_set_one_line(g_ui.user_name_input, true);
    lv_obj_set_style_bg_opa(g_ui.user_name_input, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_ui.user_name_input, 0, 0);
    lv_obj_add_event_cb(g_ui.user_name_input, textarea_focus_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_set_scrollbar_mode(g_ui.user_name_input, LV_SCROLLBAR_MODE_OFF);

    // 3. 创建密码输入行 (使用Flexbox布局)
    lv_obj_t* pass_line = lv_obj_create(g_ui.window_obj);
    lv_obj_set_size(pass_line, lv_pct(85), 48);
    lv_obj_align_to(pass_line, user_line, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_set_flex_flow(pass_line, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pass_line, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(pass_line, 1, 0);
    lv_obj_set_style_border_color(pass_line, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(pass_line, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(pass_line, 10, 0);
    lv_obj_set_style_pad_hor(pass_line, 10, 0);
    lv_obj_set_style_pad_column(pass_line, 5, 0);
    lv_obj_set_scrollbar_mode(pass_line, LV_SCROLLBAR_MODE_OFF);

    // 3.1 密码Key图标
    lv_obj_t* pass_icon = lv_label_create(pass_line);
    lv_obj_set_style_text_font(pass_icon, &lv_font_montserrat_20, 0);
    lv_label_set_text(pass_icon, LV_SYMBOL_KEYBOARD);
    lv_obj_set_style_text_color(pass_icon, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);

    // 3.2 密码输入框
    g_ui.password_input = lv_textarea_create(pass_line);
    lv_obj_set_style_text_font(g_ui.password_input, &lv_font_montserrat_20, 0);
    lv_textarea_set_placeholder_text(g_ui.password_input, "password (123456)");
    lv_obj_set_flex_grow(g_ui.password_input, 1);
    lv_textarea_set_one_line(g_ui.password_input, true);
    lv_textarea_set_password_mode(g_ui.password_input, true);
    lv_obj_set_style_bg_opa(g_ui.password_input, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_ui.password_input, 0, 0);
    lv_obj_add_event_cb(g_ui.password_input, password_focus_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_set_scrollbar_mode(g_ui.password_input, LV_SCROLLBAR_MODE_OFF);

    // 3.3 "显示/隐藏密码"图标
    g_ui.password_eye_icon = lv_label_create(pass_line);
    lv_obj_set_style_text_font(g_ui.password_eye_icon, &lv_font_montserrat_20, 0);
    lv_label_set_text(g_ui.password_eye_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(g_ui.password_eye_icon, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_flag(g_ui.password_eye_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.password_eye_icon, password_eye_click_event_cb, LV_EVENT_CLICKED, NULL);

    // 4. 创建登录按钮
    lv_obj_t * login_btn = lv_btn_create(g_ui.window_obj);
    lv_obj_set_size(login_btn, lv_pct(85), 48);
    lv_obj_align_to(login_btn, pass_line, LV_ALIGN_OUT_BOTTOM_MID, 0, 30);
    lv_obj_add_event_cb(login_btn, login_btn_click_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * login_label = lv_label_create(login_btn);
    lv_label_set_text(login_label, "登录");
    lv_obj_set_style_text_font(login_label, &my_font_yahei_18, 0);
    lv_obj_center(login_label);

    // 5. 创建虚拟键盘
    g_ui.user_name_keyboard = lv_keyboard_create(parent);
    lv_obj_add_flag(g_ui.user_name_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_ui.user_name_keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
    
    g_ui.password_keyboard = lv_keyboard_create(parent);
    lv_keyboard_set_mode(g_ui.password_keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_add_flag(g_ui.password_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_ui.password_keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);

    // 6. 创建键盘的模态覆盖层
    g_ui.keyboard_overlay = lv_obj_create(parent);
    lv_obj_set_size(g_ui.keyboard_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(g_ui.keyboard_overlay, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(g_ui.keyboard_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_ui.keyboard_overlay, keyboard_overlay_click_event_cb, LV_EVENT_CLICKED, NULL);
}

/*********************************************************************************
 * 公共函数定义
 *********************************************************************************/

/**
 * @brief 初始化登录屏幕。
 * @details 此函数作为本模块的入口，负责创建屏幕上的所有UI元素。
 * @param parent 要将此屏幕创建于其上的父对象（通常是 lv_scr_act()）。
 */
void ui_screen_login_init(lv_obj_t* parent) {    
    // 每次初始化时，清空UI句柄结构体，防止野指针
    memset(&g_ui, 0, sizeof(login_screen_ui_t));

    // 设置屏幕的全局背景色
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFECC0), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    // 调用内部函数，分别创建核心控件和消息框
    create_login_widgets(parent);
    create_message_box(parent);
}
