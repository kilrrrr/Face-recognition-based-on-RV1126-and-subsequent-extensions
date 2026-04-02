#ifndef _RV1126_DATA_PROCESS_H
#define _RV1126_DATA_PROCESS_H

#include "rkmedia_config_public.h"

typedef struct
{
    unsigned int config_id;
    unsigned int vencId;
}VENC_PROC_PARAM;

typedef struct
{
    unsigned int config_id;
    unsigned int aencId;
}AENC_PROC_PARAM;


void * camera_venc_thread(void *args);
void * get_rga_thread(void * args);
void * low_camera_venc_thread(void *args);
void * high_video_push_thread(void *args);
void * low_video_push_thread(void *args);
void * osd_venc_thread(void *args);
void * low_osd_venc_thread(void *args);

#endif