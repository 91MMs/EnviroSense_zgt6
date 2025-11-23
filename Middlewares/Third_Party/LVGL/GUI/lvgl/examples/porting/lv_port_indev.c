/**
 * @file lv_port_indev_templ.c
 *
 */

 /*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "lvgl.h"

/* 导入驱动头文件 */
#include "touch.h"
#include "lcd.h"
#include <stdio.h>

/*********************
 *      DEFINES
 *********************/
extern uint8_t g_lcd_scan_dir;    // 触摸屏设备结构体
/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/* 触摸屏 */
static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y);


/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t * indev_touchpad;    // 触摸屏

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief       初始化并注册输入设备
 * @param       无
 * @retval      无
 */
void lv_port_indev_init(void)
{
    /**
     * 
     * 在这里你可以找到 LittlevGL 支持的出入设备的实现示例:
     *  - 触摸屏
     *  - 鼠标 (支持光标)
     *  - 键盘 (仅支持按键的 GUI 用法)
     *  - 编码器 (支持的 GUI 用法仅包括: 左, 右, 按下)
     *  - 按钮 (按下屏幕上指定点的外部按钮)
     *
     *  函数 `..._read()` 只是示例
     *  你需要根据具体的硬件来完成这些函数
     */

    static lv_indev_drv_t indev_drv;

    /*------------------
     * 触摸屏
     * -----------------*/

    /* 初始化触摸屏(如果有) */
    touchpad_init();

    /* 注册触摸屏输入设备 */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);

}
/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * 触摸屏
 * -----------------*/

/**
 * @brief       初始化触摸屏
 * @param       无
 * @retval      无
 */
static void touchpad_init(void)
{
    /*Your code comes here*/
    tp_dev.init();
    
    // /* 电阻屏坐标矫正 */
    // if (key_scan(0) == KEY0_PRES)           /* KEY0按下,则执行校准程序 */
    // {
    //     lcd_clear(WHITE);                   /* 清屏 */
    //     tp_adjust();                        /* 屏幕校准 */
    //     tp_save_adjust_data();
    // }
}

/**
 * @brief       图形库的触摸屏读取回调函数
 * @param       indev_drv   : 触摸屏设备
 *   @arg       data        : 输入设备数据结构体
 * @retval      无
 */
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    /* 保存按下的坐标和状态 */
    if(touchpad_is_pressed())
    {
        lv_coord_t raw_x, raw_y;
        touchpad_get_xy(&raw_x, &raw_y);
        
        // 如果旋转了屏幕，默认为L2R_U2D，则需要调整坐标
        if (g_lcd_scan_dir == R2L_D2U) 
        {
            last_x = lcddev.width - 1 - raw_x;
            last_y = lcddev.height - 1 - raw_y;
        } 
        else 
        {
            last_x = raw_x;
            last_y = raw_y;
        }

        data->state = LV_INDEV_STATE_PR;
    } 
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    /* 设置最后按下的坐标 */
    data->point.x = last_x;
    data->point.y = last_y;
}

/**
 * @brief       获取触摸屏设备的状态
 * @param       无
 * @retval      返回触摸屏设备是否被按下
 */
static bool touchpad_is_pressed(void)
{
    /*Your code comes here*/
    tp_dev.scan(0);

    if (tp_dev.sta & TP_PRES_DOWN)
    {
        return true;
    }

    return false;
}

/**
 * @brief       在触摸屏被按下的时候读取 x、y 坐标
 * @param       x   : x坐标的指针
 *   @arg       y   : y坐标的指针
 * @retval      无
 */
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y)
{
    /*Your code comes here*/
    (*x) = tp_dev.x[0];
    (*y) = tp_dev.y[0];
}


#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
