#ifndef _RV1126_ISP_CONTROL_LAYER_H
#define _RV1126_ISP_CONTROL_LAYER_H

#include "rv1126_isp_function.h"

typedef enum
{
    RV1126_ISP_DAY_NIGHT_MODE_AUTO = 0,
    RV1126_ISP_DAY_NIGHT_MODE_DAY,
    RV1126_ISP_DAY_NIGHT_MODE_NIGHT
} RV1126_ISP_DAY_NIGHT_MODE;

typedef enum
{
    RV1126_ISP_SCENE_PROFILE_DAY = 0,
    RV1126_ISP_SCENE_PROFILE_NIGHT,
    RV1126_ISP_SCENE_PROFILE_BACKLIGHT
} RV1126_ISP_SCENE_PROFILE;

typedef struct
{
    unsigned int fps;
    unsigned int control_interval_ms;
    unsigned int scene_confirm_count;
    unsigned int ldch_level_day;
    unsigned int ldch_level_night;
    unsigned int brightness_day;
    unsigned int brightness_night;
    unsigned int contrast_day;
    unsigned int contrast_night;
    unsigned int saturation_day;
    unsigned int saturation_night;
    float day_gain_min;
    float day_gain_max;
    float day_time_min;
    float day_time_max;
    float night_gain_min;
    float night_gain_max;
    float night_time_min;
    float night_time_max;
    float night_enter_luma;
    float night_exit_luma;
    float night_enter_lux;
    float night_exit_lux;
    float backlight_enter_deviation;
    float backlight_enter_mean_luma_max;
    int auto_control_enabled;
    int night_gray_mode_enabled;
    int backlight_enable_wdr;
    RV1126_ISP_DAY_NIGHT_MODE day_night_mode;
    rk_aiq_wb_scene_t night_wb_scene;
} RV1126_ISP_CONTROL_CONFIG;

typedef struct
{
    RV1126_ISP_SCENE_PROFILE current_profile;
    RV1126_ISP_DAY_NIGHT_MODE configured_day_night_mode;
    rk_aiq_working_mode_t current_wdr_mode;
    rk_aiq_gray_mode_t current_gray_mode;
    float mean_luma;
    float luma_deviation;
    float global_env_lux;
    float integration_time_us;
    float analog_gain;
    float color_temperature;
    int awb_converged;
    unsigned int ldch_level;
    unsigned int brightness;
    unsigned int contrast;
    unsigned int saturation;
} RV1126_ISP_METADATA;

void set_default_rv1126_isp_control_config(RV1126_ISP_CONTROL_CONFIG *config);
int init_rv1126_isp_control_layer(const RV1126_ISP_CONTROL_CONFIG *config);
int start_rv1126_isp_control_service();
int stop_rv1126_isp_control_service();
int get_rv1126_isp_metadata(RV1126_ISP_METADATA *metadata);

#endif
