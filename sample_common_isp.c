// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if 1

#include "sample_common.h"
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static rk_aiq_sys_ctx_t *g_aiq_ctx;

RK_S32 SAMPLE_COMM_ISP_Init(rk_aiq_working_mode_t WDRMode, RK_BOOL bFECEnable) {
  const char *iq_file_dir = "/etc/iqfiles/";
  setlinebuf(stdout);

  rk_aiq_sys_ctx_t *aiq_ctx;
  rk_aiq_static_info_t aiq_static_info;
  rk_aiq_uapi_sysctl_enumStaticMetas(0, &aiq_static_info);

  printf("sensor_name is %s, iqfiles is %s\n",
         aiq_static_info.sensor_info.sensor_name, iq_file_dir);

  aiq_ctx = rk_aiq_uapi_sysctl_init(aiq_static_info.sensor_info.sensor_name,
                                    iq_file_dir, NULL, NULL);

  printf("rk_aiq_uapi_sysctl_init bFECEnable %d\n", bFECEnable);

  rk_aiq_uapi_setFecEn(aiq_ctx, bFECEnable);

  printf("rk_aiq_uapi_setFecEn\n");
  if (rk_aiq_uapi_sysctl_prepare(aiq_ctx, 0, 0, WDRMode)) {
    printf("rkaiq engine prepare failed !\n");
    return -1;
  }
  printf("rk_aiq_uapi_sysctl_init/prepare succeed\n");
  g_aiq_ctx = aiq_ctx;
  return 0;
}

RK_VOID SAMPLE_COMM_ISP_Stop(void) {
  if (!g_aiq_ctx)
    return;

  printf("rk_aiq_uapi_sysctl_stop enter\n");
  rk_aiq_uapi_sysctl_stop(g_aiq_ctx, false);
  printf("rk_aiq_uapi_sysctl_deinit enter\n");
  rk_aiq_uapi_sysctl_deinit(g_aiq_ctx);
  printf("rk_aiq_uapi_sysctl_deinit exit\n");
  g_aiq_ctx = NULL;
}

RK_S32 SAMPLE_COMM_ISP_Run(void) {
  if (!g_aiq_ctx)
    return -1;

  if (rk_aiq_uapi_sysctl_start(g_aiq_ctx)) {
    printf("rk_aiq_uapi_sysctl_start  failed\n");
    return -1;
  }
  printf("rk_aiq_uapi_sysctl_start succeed\n");
  return 0;
}

RK_VOID SAMPLE_COMM_ISP_DumpExpInfo(rk_aiq_working_mode_t WDRMode) {
  char aStr[128] = {'\0'};
  Uapi_ExpQueryInfo_t stExpInfo;
  rk_aiq_wb_cct_t stCCT;

  rk_aiq_user_api_ae_queryExpResInfo(g_aiq_ctx, &stExpInfo);
  rk_aiq_user_api_awb_GetCCT(g_aiq_ctx, &stCCT);

  if (WDRMode == RK_AIQ_WORKING_MODE_NORMAL) {
    sprintf(aStr, "M:%.0f-%.1f LM:%.1f CT:%.1f",
            stExpInfo.CurExpInfo.LinearExp.exp_real_params.integration_time *
                1000 * 1000,
            stExpInfo.CurExpInfo.LinearExp.exp_real_params.analog_gain,
            stExpInfo.MeanLuma, stCCT.CCT);
  } else {
    sprintf(aStr, "S:%.0f-%.1f M:%.0f-%.1f L:%.0f-%.1f SLM:%.1f MLM:%.1f "
                  "LLM:%.1f CT:%.1f",
            stExpInfo.CurExpInfo.HdrExp[0].exp_real_params.integration_time *
                1000 * 1000,
            stExpInfo.CurExpInfo.HdrExp[0].exp_real_params.analog_gain,
            stExpInfo.CurExpInfo.HdrExp[1].exp_real_params.integration_time *
                1000 * 1000,
            stExpInfo.CurExpInfo.HdrExp[1].exp_real_params.analog_gain,
            stExpInfo.CurExpInfo.HdrExp[2].exp_real_params.integration_time *
                1000 * 1000,
            stExpInfo.CurExpInfo.HdrExp[2].exp_real_params.analog_gain,
            stExpInfo.HdrMeanLuma[0], stExpInfo.HdrMeanLuma[1],
            stExpInfo.HdrMeanLuma[2], stCCT.CCT);
  }
  printf("isp exp dump: %s\n", aStr);
}

RK_VOID SAMPLE_COMM_ISP_SetFrameRate(RK_U32 uFps) {
  if (!g_aiq_ctx)
    return;

  printf("SAMPLE_COMM_ISP_SetFrameRate start %d\n", uFps);

  frameRateInfo_t info;
  info.mode = OP_MANUAL;
  info.fps = uFps;
  rk_aiq_uapi_setFrameRate(g_aiq_ctx, info);

  printf("SAMPLE_COMM_ISP_SetFrameRate %d\n", uFps);
}

RK_VOID SAMPLE_COMM_ISP_SetLDCHLevel(RK_U32 level) {
  if (!g_aiq_ctx)
    return;
  rk_aiq_uapi_setLdchEn(g_aiq_ctx, level > 0);
  if (level > 0 && level <= 255)
    rk_aiq_uapi_setLdchCorrectLevel(g_aiq_ctx, level);
}

/*only support switch between HDR and normal*/
RK_VOID SAMPLE_COMM_ISP_SetWDRModeDyn(rk_aiq_working_mode_t WDRMode) {
  if (!g_aiq_ctx)
    return;

  rk_aiq_uapi_sysctl_swWorkingModeDyn(g_aiq_ctx, WDRMode);
}

RK_S32 SAMPLE_COMM_ISP_SetExposureMode(opMode_t mode) {
  if (!g_aiq_ctx)
    return -1;

  return rk_aiq_uapi_setExpMode(g_aiq_ctx, mode);
}

RK_S32 SAMPLE_COMM_ISP_SetManualExposure(float gain, float time) {
  if (!g_aiq_ctx)
    return -1;

  return rk_aiq_uapi_setManualExp(g_aiq_ctx, gain, time);
}

RK_S32 SAMPLE_COMM_ISP_SetExposureRange(float gain_min, float gain_max, float time_min, float time_max) {
  if (!g_aiq_ctx)
    return -1;

  paRange_t gain_range;
  paRange_t time_range;
  gain_range.min = gain_min;
  gain_range.max = gain_max;
  time_range.min = time_min;
  time_range.max = time_max;

  if (rk_aiq_uapi_setExpGainRange(g_aiq_ctx, &gain_range) != XCAM_RETURN_NO_ERROR)
    return -1;

  if (rk_aiq_uapi_setExpTimeRange(g_aiq_ctx, &time_range) != XCAM_RETURN_NO_ERROR)
    return -1;

  return 0;
}

RK_S32 SAMPLE_COMM_ISP_SetWBMode(opMode_t mode) {
  if (!g_aiq_ctx)
    return -1;

  return rk_aiq_uapi_setWBMode(g_aiq_ctx, mode);
}

RK_S32 SAMPLE_COMM_ISP_SetWBScene(rk_aiq_wb_scene_t scene) {
  if (!g_aiq_ctx)
    return -1;

  return rk_aiq_uapi_setMWBScene(g_aiq_ctx, scene);
}

RK_S32 SAMPLE_COMM_ISP_SetGrayMode(rk_aiq_gray_mode_t mode) {
  if (!g_aiq_ctx)
    return -1;

  return rk_aiq_uapi_setGrayMode(g_aiq_ctx, mode);
}

RK_S32 SAMPLE_COMM_ISP_SetBrightness(RK_U32 level) {
  if (!g_aiq_ctx)
    return -1;

  return rk_aiq_uapi_setBrightness(g_aiq_ctx, level);
}

RK_S32 SAMPLE_COMM_ISP_SetContrast(RK_U32 level) {
  if (!g_aiq_ctx)
    return -1;

  return rk_aiq_uapi_setContrast(g_aiq_ctx, level);
}

RK_S32 SAMPLE_COMM_ISP_SetSaturation(RK_U32 level) {
  if (!g_aiq_ctx)
    return -1;

  return rk_aiq_uapi_setSaturation(g_aiq_ctx, level);
}

RK_S32 SAMPLE_COMM_ISP_QueryExpInfo(Uapi_ExpQueryInfo_t *exp_info) {
  if (!g_aiq_ctx || !exp_info)
    return -1;

  return rk_aiq_user_api_ae_queryExpResInfo(g_aiq_ctx, exp_info);
}

RK_S32 SAMPLE_COMM_ISP_QueryWBInfo(rk_aiq_wb_querry_info_t *wb_info) {
  if (!g_aiq_ctx || !wb_info)
    return -1;

  return rk_aiq_user_api_awb_QueryWBInfo(g_aiq_ctx, wb_info);
}

RK_S32 SAMPLE_COMM_ISP_QueryCCT(rk_aiq_wb_cct_t *cct_info) {
  if (!g_aiq_ctx || !cct_info)
    return -1;

  return rk_aiq_user_api_awb_GetCCT(g_aiq_ctx, cct_info);
}

#endif
