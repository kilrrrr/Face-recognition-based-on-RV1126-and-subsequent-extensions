#ifndef _ROCKX_VIDEO_CONFERENCE_MODULE_H
#define _ROCKX_VIDEO_CONFERENCE_MODULE_H

#include "rkmedia_config_public.h"

#define ROCKX_VIDEO_CONFERENCE_VENC_ID 2
#define ROCKX_VIDEO_CONFERENCE_OUTPUT_PATH "./video_conference_ai_venc.h264"
#define ROCKX_VIDEO_CONFERENCE_OUTPUT_PATH_LENGTH 256
#define ROCKX_VIDEO_CONFERENCE_ANALYSIS_WIDTH 640
#define ROCKX_VIDEO_CONFERENCE_ANALYSIS_HEIGHT 360

typedef struct
{
    unsigned int vi_id;
    unsigned int venc_id;
    unsigned int width;
    unsigned int height;
    unsigned int analysis_width;
    unsigned int analysis_height;
    float track_smooth_alpha;
    unsigned int max_track_lost_frames;
    char output_path[ROCKX_VIDEO_CONFERENCE_OUTPUT_PATH_LENGTH];
} ROCKX_VIDEO_CONFERENCE_PARAM;

int init_rockx_video_conference_venc_module();

void *rockx_video_conference_thread(void *args);
void *rockx_video_conference_venc_thread(void *args);

#endif
