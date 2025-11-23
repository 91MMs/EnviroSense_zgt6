/**
 * @file      ui_screen_sensors_details.c
 * @brief     传感器数据显示详情页面 (支持单/双曲线)
 * @details   使用LVGL展示传感器实时值、统计信息及历史曲线。
 *            图表数据采用整数缩放方式以保留浮点精度（SCALE_FACTOR）。
 * @author    MmsY (改进与注释)
 * @date      2025
 */

#include "ui_manager.h"
#include "ui_screen_sensors_details.h"
#include "sensor_task.h"
#include "ui_comp_header.h"  // [NEW] 引入顶部栏组件

LV_FONT_DECLARE(my_font_yahei_24);

/* -----------------------------------------------------------
 * 常量与类型
 * ----------------------------------------------------------- */
#define SCALE_FACTOR 10
#define LV_COORD_T_MAX 32767
#define LV_COORD_T_MIN -32768

/**
 * @brief 传感器详情页面UI控件集合
 */
typedef struct {
    ui_header_t* header;               // [NEW] 顶部栏组件句柄
    lv_obj_t* realtime_val_label;      // 实时数值显示
    lv_obj_t* min_val_label;           // 最小值标签
    lv_obj_t* max_val_label;           // 最大值标签
    lv_obj_t* avg_val_label;           // 平均值标签
    lv_obj_t* chart;                   // 历史曲线图表对象
    lv_chart_series_t* series_primary;   // 主曲线系列（例如温度）
    lv_chart_series_t* series_secondary; // 次曲线系列（例如湿度）
    lv_timer_t* update_timer;          // 定时器：用于定期刷新界面
} sensors_details_ui_t;  // [CHANGED] 重命名结构体

/* 模块静态变量 */
static sensors_details_ui_t g_sensors_details_ui;  // [CHANGED] 重命名变量
static SensorType_t g_active_sensor_type;  // [CHANGED] 重命名变量

/* 均为 SENSOR_HISTORY_SIZE 长度的坐标缓存（整数，已缩放） */
static lv_coord_t primary_coord_buffer[SENSOR_HISTORY_SIZE];
static lv_coord_t secondary_coord_buffer[SENSOR_HISTORY_SIZE];

/* -----------------------------------------------------------
 * 前向声明
 * ----------------------------------------------------------- */
static void back_btn_event_cb(lv_event_t* e);
static void convert_float_to_scaled_coords(const float* src, lv_coord_t* dst, uint16_t count, int scale);
static void chart_draw_event_cb(lv_event_t * e);
static void sensors_details_update_timer_cb(lv_timer_t* timer);  // [CHANGED] 重命名函数

/* -----------------------------------------------------------
 * 回调与工具函数实现
 * ----------------------------------------------------------- */

/**
 * @brief 返回按钮事件回调
 */
static void back_btn_event_cb(lv_event_t* e)
{
    (void)e;
    ui_load_previous_screen();
}

/**
 * @brief 将浮点数组转换为缩放后的整数坐标数组
 */
static void convert_float_to_scaled_coords(const float* src, lv_coord_t* dst, uint16_t count, int scale)
{
    for (uint16_t i = 0; i < count; i++) {
        dst[i] = (lv_coord_t)(src[i] * scale);
    }
}

/**
 * @brief 图表绘制事件回调（用于自定义坐标轴刻度文本）
 */
static void chart_draw_event_cb(lv_event_t * e)
{
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);

    if(!lv_obj_draw_part_check_type(dsc, &lv_chart_class, LV_CHART_DRAW_PART_TICK_LABEL)) {
        return;
    }

    if(dsc->p1 && dsc->p2) {
        if(dsc->id == LV_CHART_AXIS_PRIMARY_X) {
            int total_seconds = SENSOR_HISTORY_SIZE / 2;
            int num_ticks = 4;
            int tick_index = (int)dsc->value;
            int seconds_per_interval = total_seconds / (num_ticks - 1);
            int time_val = tick_index * seconds_per_interval;

            if (tick_index == num_ticks - 1) {
                snprintf(dsc->text, dsc->text_length, "Now");
            } else {
                snprintf(dsc->text, dsc->text_length, "-%ds", total_seconds - time_val);
            }
        }
        else if(dsc->id == LV_CHART_AXIS_PRIMARY_Y || dsc->id == LV_CHART_AXIS_SECONDARY_Y) {
            lv_coord_t value = dsc->value;
            snprintf(dsc->text, dsc->text_length, "%.1f", (float)value / SCALE_FACTOR);
        }
    }
}

/**
 * @brief 界面周期性刷新定时器回调
 */
static void sensors_details_update_timer_cb(lv_timer_t* timer)  // [CHANGED] 重命名函数
{
    (void)timer;
    SensorData_t data;
    SensorStats_t primary_stats;
    SensorStats_t secondary_stats;
    const float* history_primary = NULL;
    const float* history_secondary = NULL;
    uint16_t history_count = 0;

    /* 1. 获取最新传感器数据并显示实时数值 */
    if (SensorTask_GetSensorData(g_active_sensor_type, &data) && data.is_valid) {
        if (g_active_sensor_type == SENSOR_TYPE_SHT30) {
            lv_label_set_text_fmt(g_sensors_details_ui.realtime_val_label, "%.1f °C / %.1f %%RH",
                                  data.values.sht30.temp, data.values.sht30.humi);
        } else {
            float value = 0.0f;
            if (g_active_sensor_type == SENSOR_TYPE_GY30) value = data.values.gy30.lux;
            else if (g_active_sensor_type == SENSOR_TYPE_SMOKE) value = (float)data.values.smoke.ppm;
            lv_label_set_text_fmt(g_sensors_details_ui.realtime_val_label, "%.1f", value);
        }
    }

    /* 2. 更新统计数据（Min/Max/Avg） */
    if (SensorTask_GetStats(g_active_sensor_type, &primary_stats)) {
        if (g_active_sensor_type == SENSOR_TYPE_SHT30) {
            if (SensorTask_GetSecondaryStats(g_active_sensor_type, &secondary_stats)) {
                lv_label_set_text_fmt(g_sensors_details_ui.min_val_label, 
                                      "Min: #D00000 %.1f#/ #7f7f7f %.1f# | #0000D0 %.1f#/ #7f7f7f %.1f#",
                                      primary_stats.min, primary_stats.local_min,
                                      secondary_stats.min, secondary_stats.local_min);
                lv_label_set_text_fmt(g_sensors_details_ui.max_val_label, 
                                      "Max: #D00000 %.1f#/ #7f7f7f %.1f# | #0000D0 %.1f#/ #7f7f7f %.1f#",
                                      primary_stats.max, primary_stats.local_max,
                                      secondary_stats.max, secondary_stats.local_max);
                lv_label_set_text_fmt(g_sensors_details_ui.avg_val_label, "Avg: #D00000 %.1f# / #0000D0 %.1f#",
                                      primary_stats.local_avg, secondary_stats.local_avg);
            }
        } else {
            lv_label_set_text_fmt(g_sensors_details_ui.min_val_label, 
                                  "Min: #00D000 %.1f#/ #7f7f7f %.1f#",
                                  primary_stats.min, primary_stats.local_min);
            lv_label_set_text_fmt(g_sensors_details_ui.max_val_label, 
                                  "Max: #00D000 %.1f#/ #7f7f7f %.1f#",
                                  primary_stats.max, primary_stats.local_max);
            lv_label_set_text_fmt(g_sensors_details_ui.avg_val_label, "Avg: #00D000 %.1f#",
                                  primary_stats.local_avg);
        }

        float range = primary_stats.local_max - primary_stats.local_min;
        if (range < 10) range = 20;
        float margin = range * 0.1f;

        float y_max_float = (primary_stats.local_max + margin) * SCALE_FACTOR;
        float y_min_float = (primary_stats.local_min - margin) * SCALE_FACTOR;

        y_max_float = (y_max_float >= LV_COORD_T_MAX) ? LV_COORD_T_MAX : y_max_float;
        y_min_float = (y_min_float <= LV_COORD_T_MIN) ? LV_COORD_T_MIN : y_min_float;
        y_min_float = (y_min_float >= y_max_float) ? (y_max_float - 1) : y_min_float;
        
        lv_chart_set_range(g_sensors_details_ui.chart, LV_CHART_AXIS_PRIMARY_Y, 
                          (lv_coord_t)y_min_float, (lv_coord_t)y_max_float);

        if (g_active_sensor_type == SENSOR_TYPE_SHT30) {
            range = secondary_stats.local_max - secondary_stats.local_min;
            if (range < 10) range = 10;
            margin = range * 0.1f;

            y_max_float = (secondary_stats.local_max + margin) * SCALE_FACTOR;
            y_min_float = (secondary_stats.local_min - margin) * SCALE_FACTOR;

            y_max_float = (y_max_float >= LV_COORD_T_MAX) ? LV_COORD_T_MAX : y_max_float;
            y_min_float = (y_min_float <= LV_COORD_T_MIN) ? LV_COORD_T_MIN : y_min_float;
            y_min_float = (y_min_float >= y_max_float) ? (y_max_float - 1) : y_min_float;

            lv_chart_set_range(g_sensors_details_ui.chart, LV_CHART_AXIS_SECONDARY_Y, 
                              (lv_coord_t)y_min_float, (lv_coord_t)y_max_float);
        }
    }

    /* 3. 获取历史数据并更新图表 */
    history_count = SensorTask_GetPrimaryHistory(g_active_sensor_type, &history_primary);
    if (history_count > 0 && history_primary != NULL) {
        convert_float_to_scaled_coords(history_primary, primary_coord_buffer,
                                       history_count, SCALE_FACTOR);
        lv_chart_set_ext_y_array(g_sensors_details_ui.chart, g_sensors_details_ui.series_primary, 
                                primary_coord_buffer);
        lv_chart_set_point_count(g_sensors_details_ui.chart, history_count);

        if (g_active_sensor_type == SENSOR_TYPE_SHT30) {
            SensorTask_GetSecondaryHistory(g_active_sensor_type, &history_secondary);
            if (history_secondary != NULL) {
                convert_float_to_scaled_coords(history_secondary, secondary_coord_buffer,
                                               history_count, SCALE_FACTOR);
                lv_chart_set_ext_y_array(g_sensors_details_ui.chart, g_sensors_details_ui.series_secondary, 
                                        secondary_coord_buffer);
            }
        }

        lv_chart_refresh(g_sensors_details_ui.chart);
    }
}

/* -----------------------------------------------------------
 * 界面初始化与反初始化
 * ----------------------------------------------------------- */

/**
 * @brief 初始化传感器详情页面UI
 */
void ui_screen_sensors_details_init(lv_obj_t* parent)  // [CHANGED] 重命名函数
{
    memset(&g_sensors_details_ui, 0, sizeof(sensors_details_ui_t));
    g_active_sensor_type = ui_get_active_sensor();
    const char* sensor_name = SensorType_ToString(g_active_sensor_type);

    /* Grid 布局与间距 */
    lv_obj_set_layout(parent, LV_LAYOUT_GRID);
    // lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_style_pad_gap(parent, 10, 0);
    static lv_coord_t col_dsc[] = { LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t row_dsc[] = { 70, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_set_grid_dsc_array(parent, col_dsc, row_dsc);

    /* === 1. 使用 ui_comp_header 创建顶部栏 === */
    char title_buf[32];
    snprintf(title_buf, sizeof(title_buf), "传感器: %s", sensor_name);
    
    ui_header_config_t header_config = {
        .title = title_buf,
        .show_back_btn = true,
        .show_custom_btn = false,
        .custom_btn_text = NULL,
        .back_btn_cb = back_btn_event_cb,
        .custom_btn_cb = NULL,
        .user_data = NULL,
        .show_time = true
    };
    
    g_sensors_details_ui.header = ui_comp_header_create(parent, &header_config);
    
    /* 设置顶部栏在 Grid 中的位置 */
    lv_obj_set_grid_cell(g_sensors_details_ui.header->container, 
                         LV_GRID_ALIGN_STRETCH, 0, 1,
                         LV_GRID_ALIGN_STRETCH, 0, 1);

    /* === 2. 实时数据显示面板 === */
    lv_obj_t* realtime_panel = lv_obj_create(parent);
    lv_obj_set_height(realtime_panel, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(realtime_panel, 5, 0);
    lv_obj_set_grid_cell(realtime_panel, LV_GRID_ALIGN_STRETCH, 0, 1,
                                  LV_GRID_ALIGN_STRETCH, 1, 1);
    g_sensors_details_ui.realtime_val_label = lv_label_create(realtime_panel);
    lv_obj_set_style_text_font(g_sensors_details_ui.realtime_val_label, &lv_font_montserrat_36, 0);
    lv_label_set_text(g_sensors_details_ui.realtime_val_label, "--.-");
    lv_obj_center(g_sensors_details_ui.realtime_val_label);

    /* === 3. 统计信息面板 === */
    lv_obj_t* stats_panel = lv_obj_create(parent);
    lv_obj_set_height(stats_panel, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(stats_panel, 10, 0);
    lv_obj_set_layout(stats_panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(stats_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stats_panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_grid_cell(stats_panel, LV_GRID_ALIGN_STRETCH, 0, 1,
                                  LV_GRID_ALIGN_STRETCH, 2, 1);
    
    g_sensors_details_ui.min_val_label = lv_label_create(stats_panel);
    g_sensors_details_ui.max_val_label = lv_label_create(stats_panel);
    g_sensors_details_ui.avg_val_label = lv_label_create(stats_panel);
    
    lv_label_set_recolor(g_sensors_details_ui.min_val_label, true);
    lv_label_set_recolor(g_sensors_details_ui.max_val_label, true);
    lv_label_set_recolor(g_sensors_details_ui.avg_val_label, true);
    
    lv_obj_set_style_text_font(g_sensors_details_ui.min_val_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(g_sensors_details_ui.max_val_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(g_sensors_details_ui.avg_val_label, &lv_font_montserrat_20, 0);
    
    lv_label_set_text(g_sensors_details_ui.min_val_label, "Min: --");
    lv_label_set_text(g_sensors_details_ui.max_val_label, "Max: --");
    lv_label_set_text(g_sensors_details_ui.avg_val_label, "Avg: --");

    /* === 4. 历史数据图表 === */
    lv_obj_t* chart_container = lv_obj_create(parent);
    lv_obj_remove_style_all(chart_container);
    lv_obj_set_grid_cell(chart_container, LV_GRID_ALIGN_STRETCH, 0, 1,
                                        LV_GRID_ALIGN_STRETCH, 3, 1);

    lv_obj_set_style_pad_left(chart_container, 50, 0);
    lv_obj_set_style_pad_right(chart_container, 50, 0);
    lv_obj_set_style_pad_bottom(chart_container, 30, 0);
    lv_obj_set_style_pad_top(chart_container, 10, 0);

    g_sensors_details_ui.chart = lv_chart_create(chart_container);
    lv_obj_set_size(g_sensors_details_ui.chart, LV_PCT(100), LV_PCT(100));
    lv_obj_center(g_sensors_details_ui.chart);

    lv_chart_set_type(g_sensors_details_ui.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_sensors_details_ui.chart, SENSOR_HISTORY_SIZE);
    lv_obj_add_event_cb(g_sensors_details_ui.chart, chart_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

    lv_obj_set_style_bg_color(g_sensors_details_ui.chart, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_sensors_details_ui.chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_sensors_details_ui.chart, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_sensors_details_ui.chart, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_border_side(g_sensors_details_ui.chart, LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    
    lv_chart_set_div_line_count(g_sensors_details_ui.chart, 5, 10);
    lv_obj_set_style_line_width(g_sensors_details_ui.chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_dash_width(g_sensors_details_ui.chart, 2, LV_PART_MAIN);
    lv_obj_set_style_line_dash_gap(g_sensors_details_ui.chart, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(g_sensors_details_ui.chart, lv_color_hex(0xECECEC), LV_PART_MAIN);

    lv_obj_set_style_text_font(g_sensors_details_ui.chart, &lv_font_montserrat_14, LV_PART_TICKS);
    lv_obj_set_style_text_color(g_sensors_details_ui.chart, lv_color_black(), LV_PART_TICKS);

    lv_chart_set_axis_tick(g_sensors_details_ui.chart, LV_CHART_AXIS_PRIMARY_X, 5, 2, 4, 2, true, 40);

    if (g_active_sensor_type == SENSOR_TYPE_SHT30) {
        lv_chart_set_axis_tick(g_sensors_details_ui.chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 6, 2, true, 50);
        lv_chart_set_axis_tick(g_sensors_details_ui.chart, LV_CHART_AXIS_SECONDARY_Y, 5, 2, 6, 2, true, 50);
        g_sensors_details_ui.series_primary = lv_chart_add_series(g_sensors_details_ui.chart, 
                                                                  lv_palette_main(LV_PALETTE_RED), 
                                                                  LV_CHART_AXIS_PRIMARY_Y);
        g_sensors_details_ui.series_secondary = lv_chart_add_series(g_sensors_details_ui.chart, 
                                                                    lv_palette_main(LV_PALETTE_BLUE), 
                                                                    LV_CHART_AXIS_SECONDARY_Y);
    } else {
        lv_chart_set_axis_tick(g_sensors_details_ui.chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 5, 2, true, 50);
        g_sensors_details_ui.series_primary = lv_chart_add_series(g_sensors_details_ui.chart, 
                                                                  lv_palette_main(LV_PALETTE_GREEN), 
                                                                  LV_CHART_AXIS_PRIMARY_Y);
        g_sensors_details_ui.series_secondary = NULL;
    }

    lv_obj_set_style_line_width(g_sensors_details_ui.chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(g_sensors_details_ui.chart, 5, LV_PART_INDICATOR);

    /* === 5. 启动定时器 === */
    g_sensors_details_ui.update_timer = lv_timer_create(sensors_details_update_timer_cb, 500, NULL);
}

/**
 * @brief 反初始化传感器详情页面
 */
void ui_screen_sensors_details_deinit(void)  // [CHANGED] 重命名函数
{
    /* 销毁顶部栏 */
    if (g_sensors_details_ui.header) {
        ui_comp_header_destroy(g_sensors_details_ui.header);
        g_sensors_details_ui.header = NULL;
    }
    
    /* 删除定时器 */
    if (g_sensors_details_ui.update_timer) {
        lv_timer_del(g_sensors_details_ui.update_timer);
        g_sensors_details_ui.update_timer = NULL;
    }
}
