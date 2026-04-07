#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "rkmedia_common.h"

#if 1
#include <rk_aiq_user_api_imgproc.h>
#include <rk_aiq_user_api_sysctl.h>
/*
 * stream on:
 * 1) ISP Init
 * 2) ISP Stop
 * 3) VI enable and stream on
 *
 * stream off:
 * 4) ISP Stop
 * 5) VI disable
 */
/*
typedef enum {
 RK_AIQ_WORKING_MODE_NORMAL,
 RK_AIQ_WORKING_MODE_ISP_HDR2    = 0x10,
 RK_AIQ_WORKING_MODE_ISP_HDR3    = 0x20,
 //RK_AIQ_WORKING_MODE_SENSOR_HDR = 10, // sensor built-in hdr mode
} rk_aiq_working_mode_t;
*/
RK_S32 SAMPLE_COMM_ISP_Init(rk_aiq_working_mode_t WDRMode, RK_BOOL bFECEnable);
RK_VOID SAMPLE_COMM_ISP_Stop(RK_VOID);
RK_S32 SAMPLE_COMM_ISP_Run(RK_VOID); // isp stop before vi streamoff

RK_VOID SAMPLE_COMM_ISP_DumpExpInfo(rk_aiq_working_mode_t WDRMode);
RK_VOID SAMPLE_COMM_ISP_SetFrameRate(RK_U32 uFps);
RK_VOID SAMPLE_COMM_ISP_SetLDCHLevel(RK_U32 level);
RK_VOID SAMPLE_COMM_ISP_SetWDRModeDyn(rk_aiq_working_mode_t WDRMode);
RK_S32 SAMPLE_COMM_ISP_SetExposureMode(opMode_t mode);
RK_S32 SAMPLE_COMM_ISP_SetManualExposure(float gain, float time);
RK_S32 SAMPLE_COMM_ISP_SetExposureRange(float gain_min, float gain_max, float time_min, float time_max);
RK_S32 SAMPLE_COMM_ISP_SetWBMode(opMode_t mode);
RK_S32 SAMPLE_COMM_ISP_SetWBScene(rk_aiq_wb_scene_t scene);
RK_S32 SAMPLE_COMM_ISP_SetGrayMode(rk_aiq_gray_mode_t mode);
RK_S32 SAMPLE_COMM_ISP_SetBrightness(RK_U32 level);
RK_S32 SAMPLE_COMM_ISP_SetContrast(RK_U32 level);
RK_S32 SAMPLE_COMM_ISP_SetSaturation(RK_U32 level);
RK_S32 SAMPLE_COMM_ISP_QueryExpInfo(Uapi_ExpQueryInfo_t *exp_info);
RK_S32 SAMPLE_COMM_ISP_QueryWBInfo(rk_aiq_wb_querry_info_t *wb_info);
RK_S32 SAMPLE_COMM_ISP_QueryCCT(rk_aiq_wb_cct_t *cct_info);
#endif
