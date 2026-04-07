#include "rv1126_isp_control_layer.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    RV1126_ISP_CONTROL_CONFIG config;
    RV1126_ISP_METADATA metadata;
    pthread_t control_thread;
    pthread_mutex_t lock;
    RV1126_ISP_SCENE_PROFILE pending_profile;
    unsigned int pending_profile_hits;
    int initialized;
    int control_thread_running;
    int control_thread_started;
} RV1126_ISP_RUNTIME_STATE;

static RV1126_ISP_RUNTIME_STATE g_isp_runtime;

static void sleep_ms(unsigned int ms)
{
    usleep(ms * 1000);
}

static const char *get_scene_profile_name(RV1126_ISP_SCENE_PROFILE profile)
{
    switch (profile)
    {
    case RV1126_ISP_SCENE_PROFILE_DAY:
        return "day";
    case RV1126_ISP_SCENE_PROFILE_NIGHT:
        return "night";
    case RV1126_ISP_SCENE_PROFILE_BACKLIGHT:
        return "backlight";
    default:
        return "unknown";
    }
}

static void update_applied_metadata(RV1126_ISP_SCENE_PROFILE profile,
                                    rk_aiq_working_mode_t wdr_mode,
                                    rk_aiq_gray_mode_t gray_mode,
                                    unsigned int ldch_level,
                                    unsigned int brightness,
                                    unsigned int contrast,
                                    unsigned int saturation)
{
    g_isp_runtime.metadata.current_profile = profile;
    g_isp_runtime.metadata.current_wdr_mode = wdr_mode;
    g_isp_runtime.metadata.current_gray_mode = gray_mode;
    g_isp_runtime.metadata.ldch_level = ldch_level;
    g_isp_runtime.metadata.brightness = brightness;
    g_isp_runtime.metadata.contrast = contrast;
    g_isp_runtime.metadata.saturation = saturation;
}

static int apply_scene_profile_locked(RV1126_ISP_SCENE_PROFILE profile)
{
    const RV1126_ISP_CONTROL_CONFIG *config = &g_isp_runtime.config;
    rk_aiq_working_mode_t target_wdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
    rk_aiq_gray_mode_t target_gray_mode = RK_AIQ_GRAY_MODE_OFF;
    unsigned int ldch_level = config->ldch_level_day;
    unsigned int brightness = config->brightness_day;
    unsigned int contrast = config->contrast_day;
    unsigned int saturation = config->saturation_day;
    float gain_min = config->day_gain_min;
    float gain_max = config->day_gain_max;
    float time_min = config->day_time_min;
    float time_max = config->day_time_max;
    opMode_t wb_mode = OP_AUTO;

    switch (profile)
    {
    case RV1126_ISP_SCENE_PROFILE_NIGHT:
        ldch_level = config->ldch_level_night;
        brightness = config->brightness_night;
        contrast = config->contrast_night;
        saturation = config->saturation_night;
        gain_min = config->night_gain_min;
        gain_max = config->night_gain_max;
        time_min = config->night_time_min;
        time_max = config->night_time_max;
        target_gray_mode = config->night_gray_mode_enabled ? RK_AIQ_GRAY_MODE_ON : RK_AIQ_GRAY_MODE_OFF;
        wb_mode = config->night_gray_mode_enabled ? OP_AUTO : OP_MANUAL;
        break;
    case RV1126_ISP_SCENE_PROFILE_BACKLIGHT:
        target_wdr_mode = config->backlight_enable_wdr ? RK_AIQ_WORKING_MODE_ISP_HDR2 : RK_AIQ_WORKING_MODE_NORMAL;
        brightness = config->brightness_day + 6;
        contrast = config->contrast_day + 4;
        saturation = config->saturation_day;
        break;
    case RV1126_ISP_SCENE_PROFILE_DAY:
    default:
        break;
    }

    if (SAMPLE_COMM_ISP_SetExposureMode(OP_AUTO) != 0)
        printf("isp control set exposure mode failed\n");

    if (SAMPLE_COMM_ISP_SetExposureRange(gain_min, gain_max, time_min, time_max) != 0)
        printf("isp control set exposure range failed\n");

    if (SAMPLE_COMM_ISP_SetWBMode(wb_mode) != 0)
        printf("isp control set wb mode failed\n");

    if (wb_mode == OP_MANUAL && SAMPLE_COMM_ISP_SetWBScene(config->night_wb_scene) != 0)
        printf("isp control set wb scene failed\n");

    SAMPLE_COMM_ISP_SetWDRModeDyn(target_wdr_mode);

    if (SAMPLE_COMM_ISP_SetGrayMode(target_gray_mode) != 0)
        printf("isp control set gray mode failed\n");

    SAMPLE_COMM_ISP_SetLDCHLevel(ldch_level);

    if (SAMPLE_COMM_ISP_SetBrightness(brightness) != 0)
        printf("isp control set brightness failed\n");

    if (SAMPLE_COMM_ISP_SetContrast(contrast) != 0)
        printf("isp control set contrast failed\n");

    if (SAMPLE_COMM_ISP_SetSaturation(saturation) != 0)
        printf("isp control set saturation failed\n");

    update_applied_metadata(profile, target_wdr_mode, target_gray_mode, ldch_level, brightness, contrast, saturation);

    printf("isp control apply profile: %s\n", get_scene_profile_name(profile));
    return 0;
}

static RV1126_ISP_SCENE_PROFILE evaluate_target_profile_locked(const Uapi_ExpQueryInfo_t *exp_info)
{
    const RV1126_ISP_CONTROL_CONFIG *config = &g_isp_runtime.config;

    if (config->day_night_mode == RV1126_ISP_DAY_NIGHT_MODE_DAY)
        return RV1126_ISP_SCENE_PROFILE_DAY;

    if (config->day_night_mode == RV1126_ISP_DAY_NIGHT_MODE_NIGHT)
        return RV1126_ISP_SCENE_PROFILE_NIGHT;

    if (g_isp_runtime.metadata.current_profile == RV1126_ISP_SCENE_PROFILE_NIGHT)
    {
        if (exp_info->MeanLuma < config->night_exit_luma || exp_info->GlobalEnvLux < config->night_exit_lux)
            return RV1126_ISP_SCENE_PROFILE_NIGHT;
    }
    else
    {
        if (exp_info->MeanLuma <= config->night_enter_luma || exp_info->GlobalEnvLux <= config->night_enter_lux)
            return RV1126_ISP_SCENE_PROFILE_NIGHT;
    }

    if (exp_info->LumaDeviation >= config->backlight_enter_deviation &&
        exp_info->MeanLuma <= config->backlight_enter_mean_luma_max)
        return RV1126_ISP_SCENE_PROFILE_BACKLIGHT;

    return RV1126_ISP_SCENE_PROFILE_DAY;
}

static void refresh_query_metadata_locked(const Uapi_ExpQueryInfo_t *exp_info, const rk_aiq_wb_cct_t *cct_info, const rk_aiq_wb_querry_info_t *wb_info)
{
    float integration_time = exp_info->CurExpInfo.LinearExp.exp_real_params.integration_time;
    float analog_gain = exp_info->CurExpInfo.LinearExp.exp_real_params.analog_gain;

    if (g_isp_runtime.metadata.current_wdr_mode != RK_AIQ_WORKING_MODE_NORMAL)
    {
        integration_time = exp_info->CurExpInfo.HdrExp[1].exp_real_params.integration_time;
        analog_gain = exp_info->CurExpInfo.HdrExp[1].exp_real_params.analog_gain;
    }

    g_isp_runtime.metadata.mean_luma = exp_info->MeanLuma;
    g_isp_runtime.metadata.luma_deviation = exp_info->LumaDeviation;
    g_isp_runtime.metadata.global_env_lux = exp_info->GlobalEnvLux;
    g_isp_runtime.metadata.integration_time_us = integration_time * 1000.0f * 1000.0f;
    g_isp_runtime.metadata.analog_gain = analog_gain;
    g_isp_runtime.metadata.color_temperature = cct_info->CCT;
    g_isp_runtime.metadata.awb_converged = wb_info->awbConverged ? 1 : 0;
}

static void *isp_control_thread(void *args)
{
    (void)args;
    pthread_detach(pthread_self());

    while (g_isp_runtime.control_thread_running)
    {
        Uapi_ExpQueryInfo_t exp_info;
        rk_aiq_wb_querry_info_t wb_info;
        rk_aiq_wb_cct_t cct_info;

        memset(&exp_info, 0, sizeof(exp_info));
        memset(&wb_info, 0, sizeof(wb_info));
        memset(&cct_info, 0, sizeof(cct_info));

        if (SAMPLE_COMM_ISP_QueryExpInfo(&exp_info) == 0 &&
            SAMPLE_COMM_ISP_QueryWBInfo(&wb_info) == 0 &&
            SAMPLE_COMM_ISP_QueryCCT(&cct_info) == 0)
        {
            pthread_mutex_lock(&g_isp_runtime.lock);
            refresh_query_metadata_locked(&exp_info, &cct_info, &wb_info);

            RV1126_ISP_SCENE_PROFILE target_profile = evaluate_target_profile_locked(&exp_info);
            if (target_profile != g_isp_runtime.metadata.current_profile)
            {
                if (target_profile == g_isp_runtime.pending_profile)
                    g_isp_runtime.pending_profile_hits += 1;
                else
                {
                    g_isp_runtime.pending_profile = target_profile;
                    g_isp_runtime.pending_profile_hits = 1;
                }

                if (g_isp_runtime.pending_profile_hits >= g_isp_runtime.config.scene_confirm_count)
                {
                    apply_scene_profile_locked(target_profile);
                    g_isp_runtime.pending_profile_hits = 0;
                }
            }
            else
            {
                g_isp_runtime.pending_profile = target_profile;
                g_isp_runtime.pending_profile_hits = 0;
            }

            printf("isp control runtime: profile=%s luma=%.2f lux=%.2f gain=%.2f cct=%.2f\n",
                   get_scene_profile_name(g_isp_runtime.metadata.current_profile),
                   g_isp_runtime.metadata.mean_luma,
                   g_isp_runtime.metadata.global_env_lux,
                   g_isp_runtime.metadata.analog_gain,
                   g_isp_runtime.metadata.color_temperature);
            pthread_mutex_unlock(&g_isp_runtime.lock);
        }
        else
        {
            printf("isp control query metadata failed\n");
        }

        sleep_ms(g_isp_runtime.config.control_interval_ms);
    }

    return NULL;
}

void set_default_rv1126_isp_control_config(RV1126_ISP_CONTROL_CONFIG *config)
{
    if (config == NULL)
        return;

    memset(config, 0, sizeof(RV1126_ISP_CONTROL_CONFIG));
    config->fps = 30;
    config->control_interval_ms = 2000;
    config->scene_confirm_count = 2;
    config->ldch_level_day = 120;
    config->ldch_level_night = 80;
    config->brightness_day = 128;
    config->brightness_night = 140;
    config->contrast_day = 128;
    config->contrast_night = 116;
    config->saturation_day = 128;
    config->saturation_night = 108;
    config->day_gain_min = 1.0f;
    config->day_gain_max = 8.0f;
    config->day_time_min = 0.0001f;
    config->day_time_max = 0.02f;
    config->night_gain_min = 1.0f;
    config->night_gain_max = 24.0f;
    config->night_time_min = 0.0002f;
    config->night_time_max = 0.04f;
    config->night_enter_luma = 25.0f;
    config->night_exit_luma = 45.0f;
    config->night_enter_lux = 8.0f;
    config->night_exit_lux = 18.0f;
    config->backlight_enter_deviation = 35.0f;
    config->backlight_enter_mean_luma_max = 90.0f;
    config->auto_control_enabled = 1;
    config->night_gray_mode_enabled = 1;
    config->backlight_enable_wdr = 1;
    config->day_night_mode = RV1126_ISP_DAY_NIGHT_MODE_AUTO;
    config->night_wb_scene = RK_AIQ_WBCT_FLUORESCENT;
}

int init_rv1126_isp_control_layer(const RV1126_ISP_CONTROL_CONFIG *config)
{
    if (g_isp_runtime.initialized)
        return 0;

    memset(&g_isp_runtime, 0, sizeof(g_isp_runtime));
    pthread_mutex_init(&g_isp_runtime.lock, NULL);
    g_isp_runtime.pending_profile = RV1126_ISP_SCENE_PROFILE_DAY;

    set_default_rv1126_isp_control_config(&g_isp_runtime.config);
    if (config != NULL)
        memcpy(&g_isp_runtime.config, config, sizeof(RV1126_ISP_CONTROL_CONFIG));

    memset(&g_isp_runtime.metadata, 0, sizeof(RV1126_ISP_METADATA));
    g_isp_runtime.metadata.configured_day_night_mode = g_isp_runtime.config.day_night_mode;

    if (init_all_isp_function() != 0)
    {
        printf("init all isp function failed\n");
        return -1;
    }

    SAMPLE_COMM_ISP_SetFrameRate(g_isp_runtime.config.fps);

    pthread_mutex_lock(&g_isp_runtime.lock);
    if (g_isp_runtime.config.day_night_mode == RV1126_ISP_DAY_NIGHT_MODE_NIGHT)
        apply_scene_profile_locked(RV1126_ISP_SCENE_PROFILE_NIGHT);
    else
        apply_scene_profile_locked(RV1126_ISP_SCENE_PROFILE_DAY);
    pthread_mutex_unlock(&g_isp_runtime.lock);

    g_isp_runtime.initialized = 1;
    printf("isp control layer init success\n");
    return 0;
}

int start_rv1126_isp_control_service()
{
    if (!g_isp_runtime.initialized)
    {
        printf("isp control layer not initialized\n");
        return -1;
    }

    if (!g_isp_runtime.config.auto_control_enabled)
    {
        printf("isp control service disabled by config\n");
        return 0;
    }

    if (g_isp_runtime.control_thread_started)
        return 0;

    g_isp_runtime.control_thread_running = 1;
    if (pthread_create(&g_isp_runtime.control_thread, NULL, isp_control_thread, NULL) != 0)
    {
        g_isp_runtime.control_thread_running = 0;
        printf("create isp control thread failed\n");
        return -1;
    }

    g_isp_runtime.control_thread_started = 1;
    printf("isp control service start success\n");
    return 0;
}

int stop_rv1126_isp_control_service()
{
    g_isp_runtime.control_thread_running = 0;
    g_isp_runtime.control_thread_started = 0;
    return 0;
}

int get_rv1126_isp_metadata(RV1126_ISP_METADATA *metadata)
{
    if (metadata == NULL || !g_isp_runtime.initialized)
        return -1;

    pthread_mutex_lock(&g_isp_runtime.lock);
    *metadata = g_isp_runtime.metadata;
    pthread_mutex_unlock(&g_isp_runtime.lock);
    return 0;
}
