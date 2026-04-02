#ifndef _ROCKX_FACE_DETECT_MODULE_H
#define _ROCKX_FACE_DETECT_MODULE_H

#include "rkmedia_config_public.h"

#define ROCKX_FACE_DETECT_VENC_ID 2 //venc（2）
#define ROCKX_FACE_DETECT_OUTPUT_PATH "./rockx_face_detect_venc.h264"//输出到.h264
#define ROCKX_FACE_DETECT_OUTPUT_PATH_LENGTH 256

typedef struct  //保存检测线程和编码保存线程所需参数
{
    unsigned int vi_id;
    unsigned int venc_id;
    unsigned int width;
    unsigned int height;
    char output_path[ROCKX_FACE_DETECT_OUTPUT_PATH_LENGTH];
} ROCKX_FACE_DETECT_PARAM;

int init_rockx_face_detect_venc_module();

void *rockx_face_detect_thread(void *args);
void *rockx_face_detect_venc_thread(void *args);

#endif
