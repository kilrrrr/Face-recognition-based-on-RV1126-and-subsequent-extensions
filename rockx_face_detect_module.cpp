#include "rockx_face_detect_module.h"

#include "rkmedia_container.h"
#include "rkmedia_module.h"

#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#if defined(__has_include)
#if __has_include(<opencv2/opencv.hpp>)
#define HAVE_OPENCV_SDK 1
#include <opencv2/opencv.hpp>
#else
#define HAVE_OPENCV_SDK 0
#endif

#if __has_include("rockx.h")
#define HAVE_ROCKX_SDK 1
#include "rockx.h"
#elif __has_include(<rockx.h>)
#define HAVE_ROCKX_SDK 1
#include <rockx.h>
#elif __has_include(<rockx/rockx.h>)
#define HAVE_ROCKX_SDK 1
#include <rockx/rockx.h>
#else
#define HAVE_ROCKX_SDK 0
#endif
#else
#define HAVE_OPENCV_SDK 0
#define HAVE_ROCKX_SDK 0
#endif

typedef struct
{
    int left;
    int top;
    int right;
    int bottom;
    float score;
} FACE_BOX;

static int get_image_buffer_size(int hor_stride, int ver_stride)
{
    return hor_stride * ver_stride * 3 / 2;
}

static void copy_strided_nv12_to_packed(const unsigned char *src,
                                        int width,
                                        int height,
                                        int hor_stride,
                                        int ver_stride,
                                        std::vector<unsigned char> &dst)
{
    int y_plane_size = width * height;
    const unsigned char *src_y = src;
    const unsigned char *src_uv = src + hor_stride * ver_stride;

    dst.resize(y_plane_size + y_plane_size / 2);
    unsigned char *dst_y = dst.data();
    unsigned char *dst_uv = dst.data() + y_plane_size;

    for (int row = 0; row < height; ++row)
        memcpy(dst_y + row * width, src_y + row * hor_stride, width);

    for (int row = 0; row < height / 2; ++row)
        memcpy(dst_uv + row * width, src_uv + row * hor_stride, width);
}

static void copy_packed_nv12_to_strided(const unsigned char *src,
                                        int width,
                                        int height,
                                        int hor_stride,
                                        int ver_stride,
                                        unsigned char *dst)
{
    int y_plane_size = width * height;
    unsigned char *dst_y = dst;
    unsigned char *dst_uv = dst + hor_stride * ver_stride;
    const unsigned char *src_y = src;
    const unsigned char *src_uv = src + y_plane_size;

    for (int row = 0; row < height; ++row)
        memcpy(dst_y + row * hor_stride, src_y + row * width, width);

    for (int row = 0; row < height / 2; ++row)
        memcpy(dst_uv + row * hor_stride, src_uv + row * width, width);
}

#if HAVE_OPENCV_SDK
static void bgr_to_nv12(const cv::Mat &bgr, std::vector<unsigned char> &nv12)
{
    cv::Mat i420;
    cv::cvtColor(bgr, i420, cv::COLOR_BGR2YUV_I420);

    int width = bgr.cols;
    int height = bgr.rows;
    int y_plane_size = width * height;
    int uv_plane_size = y_plane_size / 4;

    const unsigned char *y_plane = i420.data;
    const unsigned char *u_plane = y_plane + y_plane_size;
    const unsigned char *v_plane = u_plane + uv_plane_size;

    nv12.resize(y_plane_size + y_plane_size / 2);
    memcpy(nv12.data(), y_plane, y_plane_size);

    unsigned char *uv_plane = nv12.data() + y_plane_size;
    for (int i = 0; i < uv_plane_size; ++i)
    {
        uv_plane[i * 2] = u_plane[i];
        uv_plane[i * 2 + 1] = v_plane[i];
    }
}

static void draw_face_boxes(cv::Mat &frame, const std::vector<FACE_BOX> &boxes)
{
    for (size_t i = 0; i < boxes.size(); ++i)
    {
        const FACE_BOX &box = boxes[i];
        cv::rectangle(frame,
                      cv::Point(box.left, box.top),
                      cv::Point(box.right, box.bottom),
                      cv::Scalar(0, 255, 0),
                      2);

        char label[64] = {0};
        snprintf(label, sizeof(label), "face %.2f", box.score);
        cv::putText(frame,
                    label,
                    cv::Point(box.left, box.top > 20 ? box.top - 8 : box.top + 20),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.55,
                    cv::Scalar(0, 255, 255),
                    2);
    }

    char count_text[64] = {0};
    snprintf(count_text, sizeof(count_text), "face_count: %d", (int)boxes.size());
    cv::putText(frame,
                count_text,
                cv::Point(20, 40),
                cv::FONT_HERSHEY_SIMPLEX,
                0.9,
                cv::Scalar(0, 0, 255),
                2);
}
#endif

#if HAVE_ROCKX_SDK
class RockxFaceDetector
{
public:
    RockxFaceDetector() : handle_(0), initialized_(false) {}

    ~RockxFaceDetector()
    {
        if (initialized_)
            rockx_destroy(handle_);
    }

    int Init()
    {
#ifdef ROCKX_MODULE_FACE_DETECTION_V2
        rockx_module_t module_type = ROCKX_MODULE_FACE_DETECTION_V2;
#else
        rockx_module_t module_type = ROCKX_MODULE_FACE_DETECTION;
#endif

        rockx_ret_t ret = rockx_create(&handle_, module_type, NULL, 0);
        if (ret != ROCKX_RET_SUCCESS)
        {
            printf("rockx_create failed: %d\n", ret);
            return -1;
        }

        initialized_ = true;
        printf("rockx face detector init success\n");
        return 0;
    }

    int Detect(const unsigned char *nv12, int width, int height, std::vector<FACE_BOX> &boxes)
    {
        if (!initialized_)
            return -1;

        rockx_image_t input_image;
        memset(&input_image, 0, sizeof(input_image));
        input_image.width = width;
        input_image.height = height;
        input_image.pixel_format = ROCKX_PIXEL_FORMAT_YUV420SP_NV12;
        input_image.size = width * height * 3 / 2;
        input_image.data = (uint8_t *)nv12;

        rockx_object_array_t face_array;
        memset(&face_array, 0, sizeof(face_array));

        rockx_ret_t ret = rockx_face_detect(handle_, &input_image, &face_array, NULL);
        if (ret != ROCKX_RET_SUCCESS)
        {
            printf("rockx_face_detect failed: %d\n", ret);
            return -1;
        }

        boxes.clear();
        for (int i = 0; i < face_array.count; ++i)
        {
            FACE_BOX box;
            box.left = face_array.object[i].box.left;
            box.top = face_array.object[i].box.top;
            box.right = face_array.object[i].box.right;
            box.bottom = face_array.object[i].box.bottom;
            box.score = face_array.object[i].score;
            boxes.push_back(box);
        }

        return (int)boxes.size();
    }

private:
    rockx_handle_t handle_;
    bool initialized_;
};
#endif

int init_rockx_face_detect_venc_module()
{
    RV1126_VENC_CONFIG face_detect_venc_config;
    memset(&face_detect_venc_config, 0, sizeof(face_detect_venc_config));

    face_detect_venc_config.id = ROCKX_FACE_DETECT_VENC_ID;
    face_detect_venc_config.attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
    face_detect_venc_config.attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
    face_detect_venc_config.attr.stVencAttr.u32PicWidth = 1920;
    face_detect_venc_config.attr.stVencAttr.u32PicHeight = 1080;
    face_detect_venc_config.attr.stVencAttr.u32VirWidth = 1920;
    face_detect_venc_config.attr.stVencAttr.u32VirHeight = 1080;
    face_detect_venc_config.attr.stVencAttr.u32Profile = 66;

    face_detect_venc_config.attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    face_detect_venc_config.attr.stRcAttr.stH264Cbr.u32Gop = 25;
    face_detect_venc_config.attr.stRcAttr.stH264Cbr.u32BitRate = 1920 * 1080 * 3;
    face_detect_venc_config.attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
    face_detect_venc_config.attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 25;
    face_detect_venc_config.attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    face_detect_venc_config.attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 25;

    int ret = rkmedia_venc_init(&face_detect_venc_config);
    if (ret != 0)
    {
        printf("face detect venc init error\n");
        return -1;
    }

    RV1126_VENC_CONTAINER face_detect_venc_container;
    face_detect_venc_container.id = ROCKX_FACE_DETECT_VENC_ID;
    face_detect_venc_container.venc_id = face_detect_venc_config.id;
    set_venc_container(face_detect_venc_container.id, &face_detect_venc_container);

    printf("face detect venc init success\n");
    return 0;
}

void *rockx_face_detect_thread(void *args)
{
    pthread_detach(pthread_self());

    ROCKX_FACE_DETECT_PARAM param = *(ROCKX_FACE_DETECT_PARAM *)args;
    free(args);

#if HAVE_ROCKX_SDK
    RockxFaceDetector detector;
    detector.Init();
#else
    printf("rockx sdk header not found, face detection thread will forward raw frames only\n");
#endif

    while (1)
    {
        MEDIA_BUFFER src_mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, param.vi_id, -1);
        if (!src_mb)
        {
            printf("rockx face detect get VI buffer failed\n");
            break;
        }

        MB_IMAGE_INFO_S src_info;
        memset(&src_info, 0, sizeof(src_info));
        if (RK_MPI_MB_GetImageInfo(src_mb, &src_info) != 0)
        {
            printf("rockx face detect get image info failed\n");
            RK_MPI_MB_ReleaseBuffer(src_mb);
            continue;
        }

        int width = (int)src_info.u32Width;
        int height = (int)src_info.u32Height;
        int hor_stride = (int)(src_info.u32HorStride ? src_info.u32HorStride : src_info.u32Width);
        int ver_stride = (int)(src_info.u32VerStride ? src_info.u32VerStride : src_info.u32Height);

        if (RK_MPI_MB_BeginCPUAccess(src_mb, RK_TRUE) != 0)
        {
            printf("rockx face detect begin cpu access failed\n");
            RK_MPI_MB_ReleaseBuffer(src_mb);
            continue;
        }

        unsigned char *src_ptr = (unsigned char *)RK_MPI_MB_GetPtr(src_mb);
        std::vector<unsigned char> packed_nv12;
        copy_strided_nv12_to_packed(src_ptr, width, height, hor_stride, ver_stride, packed_nv12);

        std::vector<FACE_BOX> face_boxes;
#if HAVE_ROCKX_SDK
        detector.Detect(packed_nv12.data(), width, height, face_boxes);
#endif

        std::vector<unsigned char> output_nv12 = packed_nv12;

#if HAVE_OPENCV_SDK
        cv::Mat nv12_mat(height * 3 / 2, width, CV_8UC1, packed_nv12.data());
        cv::Mat bgr_mat;
        cv::cvtColor(nv12_mat, bgr_mat, cv::COLOR_YUV2BGR_NV12);
        draw_face_boxes(bgr_mat, face_boxes);
        bgr_to_nv12(bgr_mat, output_nv12);
#else
        if (!face_boxes.empty())
            printf("opencv header not found, detected faces will not be drawn\n");
#endif

        MB_IMAGE_INFO_S output_info;
        memset(&output_info, 0, sizeof(output_info));
        output_info.u32Width = width;
        output_info.u32Height = height;
        output_info.u32HorStride = hor_stride;
        output_info.u32VerStride = ver_stride;
        output_info.enImgType = IMAGE_TYPE_NV12;

        MEDIA_BUFFER output_mb = RK_MPI_MB_CreateImageBuffer(&output_info, RK_FALSE, 0);
        if (!output_mb)
        {
            printf("rockx face detect create output buffer failed\n");
            RK_MPI_MB_EndCPUAccess(src_mb, RK_TRUE);
            RK_MPI_MB_ReleaseBuffer(src_mb);
            continue;
        }

        if (RK_MPI_MB_BeginCPUAccess(output_mb, RK_FALSE) != 0)
        {
            printf("rockx face detect begin output cpu access failed\n");
            RK_MPI_MB_ReleaseBuffer(output_mb);
            RK_MPI_MB_EndCPUAccess(src_mb, RK_TRUE);
            RK_MPI_MB_ReleaseBuffer(src_mb);
            continue;
        }

        unsigned char *output_ptr = (unsigned char *)RK_MPI_MB_GetPtr(output_mb);
        memset(output_ptr, 0, get_image_buffer_size(hor_stride, ver_stride));
        copy_packed_nv12_to_strided(output_nv12.data(), width, height, hor_stride, ver_stride, output_ptr);
        RK_MPI_MB_SetSize(output_mb, get_image_buffer_size(hor_stride, ver_stride));
        RK_MPI_MB_SetTimestamp(output_mb, RK_MPI_MB_GetTimestamp(src_mb));
        RK_MPI_MB_EndCPUAccess(output_mb, RK_FALSE);

        if (RK_MPI_SYS_SendMediaBuffer(RK_ID_VENC, param.venc_id, output_mb) != 0)
            printf("rockx face detect send buffer to venc failed\n");

        RK_MPI_MB_ReleaseBuffer(output_mb);
        RK_MPI_MB_EndCPUAccess(src_mb, RK_TRUE);
        RK_MPI_MB_ReleaseBuffer(src_mb);
    }

    return NULL;
}

void *rockx_face_detect_venc_thread(void *args)
{
    pthread_detach(pthread_self());

    ROCKX_FACE_DETECT_PARAM param = *(ROCKX_FACE_DETECT_PARAM *)args;
    free(args);

    FILE *fp = fopen(param.output_path, "wb");
    if (fp == NULL)
    {
        printf("open face detect venc output failed: %s\n", param.output_path);
        return NULL;
    }

    while (1)
    {
        MEDIA_BUFFER mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, param.venc_id, -1);
        if (!mb)
        {
            printf("rockx face detect venc thread get buffer failed\n");
            break;
        }

        fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), fp);
        fflush(fp);
        RK_MPI_MB_ReleaseBuffer(mb);
    }

    fclose(fp);
    return NULL;
}
