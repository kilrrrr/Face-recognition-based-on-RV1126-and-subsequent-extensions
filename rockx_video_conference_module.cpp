#include "rockx_video_conference_module.h"

#include "rkmedia_container.h"
#include "rkmedia_module.h"

#include <algorithm>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
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

typedef struct
{
    FACE_BOX tracked_box;
    int lost_frames;
    bool has_box;
} FACE_TRACK_STATE;

static int get_image_buffer_size(int hor_stride, int ver_stride)
{
    return hor_stride * ver_stride * 3 / 2;
}

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static bool is_valid_face_box(const FACE_BOX &box)
{
    return box.right > box.left && box.bottom > box.top;
}

static int box_width(const FACE_BOX &box)
{
    return std::max(0, box.right - box.left);
}

static int box_height(const FACE_BOX &box)
{
    return std::max(0, box.bottom - box.top);
}

static int box_area(const FACE_BOX &box)
{
    return box_width(box) * box_height(box);
}

static float box_iou(const FACE_BOX &lhs, const FACE_BOX &rhs)
{
    int left = std::max(lhs.left, rhs.left);
    int top = std::max(lhs.top, rhs.top);
    int right = std::min(lhs.right, rhs.right);
    int bottom = std::min(lhs.bottom, rhs.bottom);

    int inter_w = std::max(0, right - left);
    int inter_h = std::max(0, bottom - top);
    int inter_area = inter_w * inter_h;
    int union_area = box_area(lhs) + box_area(rhs) - inter_area;
    if (union_area <= 0)
        return 0.0f;
    return (float)inter_area / (float)union_area;
}

static FACE_BOX scale_face_box(const FACE_BOX &box, int src_width, int src_height, int dst_width, int dst_height)
{
    FACE_BOX scaled = box;
    if (src_width <= 0 || src_height <= 0)
        return scaled;

    scaled.left = clamp_int((box.left * dst_width) / src_width, 0, dst_width - 1);
    scaled.top = clamp_int((box.top * dst_height) / src_height, 0, dst_height - 1);
    scaled.right = clamp_int((box.right * dst_width) / src_width, scaled.left + 1, dst_width);
    scaled.bottom = clamp_int((box.bottom * dst_height) / src_height, scaled.top + 1, dst_height);
    return scaled;
}

static FACE_BOX smooth_face_box(const FACE_BOX &previous, const FACE_BOX &current, float alpha)
{
    if (!is_valid_face_box(previous))
        return current;

    float valid_alpha = alpha;
    if (valid_alpha < 0.0f)
        valid_alpha = 0.0f;
    if (valid_alpha > 1.0f)
        valid_alpha = 1.0f;

    FACE_BOX smoothed;
    smoothed.left = (int)(previous.left * (1.0f - valid_alpha) + current.left * valid_alpha);
    smoothed.top = (int)(previous.top * (1.0f - valid_alpha) + current.top * valid_alpha);
    smoothed.right = (int)(previous.right * (1.0f - valid_alpha) + current.right * valid_alpha);
    smoothed.bottom = (int)(previous.bottom * (1.0f - valid_alpha) + current.bottom * valid_alpha);
    smoothed.score = current.score;
    return smoothed;
}

static FACE_BOX select_primary_face(const std::vector<FACE_BOX> &boxes, const FACE_TRACK_STATE &track_state)
{
    FACE_BOX selected = {0, 0, 0, 0, 0.0f};
    double best_metric = -1.0;

    for (size_t i = 0; i < boxes.size(); ++i)
    {
        double metric = (double)box_area(boxes[i]) + boxes[i].score * 1000.0;
        if (track_state.has_box)
            metric += box_iou(boxes[i], track_state.tracked_box) * 1000000.0;

        if (metric > best_metric)
        {
            best_metric = metric;
            selected = boxes[i];
        }
    }

    return selected;
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

static cv::Rect face_box_to_rect(const FACE_BOX &box, int width, int height, float expand_ratio)
{
    int face_width = box_width(box);
    int face_height = box_height(box);
    int expand_x = (int)(face_width * expand_ratio);
    int expand_y_top = (int)(face_height * expand_ratio * 0.9f);
    int expand_y_bottom = (int)(face_height * expand_ratio * 1.2f);

    int left = clamp_int(box.left - expand_x, 0, width - 1);
    int top = clamp_int(box.top - expand_y_top, 0, height - 1);
    int right = clamp_int(box.right + expand_x, left + 1, width);
    int bottom = clamp_int(box.bottom + expand_y_bottom, top + 1, height);

    return cv::Rect(left, top, right - left, bottom - top);
}

static void apply_face_beauty(cv::Mat &frame, const FACE_BOX &box)
{
    cv::Rect roi = face_box_to_rect(box, frame.cols, frame.rows, 0.22f);
    if (roi.width <= 0 || roi.height <= 0)
        return;

    cv::Mat face_roi = frame(roi);
    cv::Mat smooth_roi;
    cv::bilateralFilter(face_roi, smooth_roi, 9, 35.0, 35.0);

    cv::Mat beauty_roi;
    cv::addWeighted(face_roi, 0.35, smooth_roi, 0.65, 8.0, beauty_roi);

    cv::Mat detail_roi;
    cv::GaussianBlur(beauty_roi, detail_roi, cv::Size(0, 0), 1.2);
    cv::addWeighted(beauty_roi, 1.08, detail_roi, -0.08, 3.0, beauty_roi);
    beauty_roi.copyTo(face_roi);
}

static void draw_conference_overlay(cv::Mat &frame,
                                    const std::vector<FACE_BOX> &faces,
                                    const FACE_BOX *tracked_face,
                                    unsigned int analysis_width,
                                    unsigned int analysis_height,
                                    int lost_frames)
{
    for (size_t i = 0; i < faces.size(); ++i)
    {
        cv::rectangle(frame,
                      cv::Point(faces[i].left, faces[i].top),
                      cv::Point(faces[i].right, faces[i].bottom),
                      cv::Scalar(80, 170, 255),
                      1);
    }

    if (tracked_face != NULL && is_valid_face_box(*tracked_face))
    {
        cv::rectangle(frame,
                      cv::Point(tracked_face->left, tracked_face->top),
                      cv::Point(tracked_face->right, tracked_face->bottom),
                      cv::Scalar(0, 255, 0),
                      2);

        cv::putText(frame,
                    "tracked speaker",
                    cv::Point(tracked_face->left, tracked_face->top > 28 ? tracked_face->top - 10 : tracked_face->top + 26),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.7,
                    cv::Scalar(0, 255, 0),
                    2);
    }

    char mode_text[128] = {0};
    snprintf(mode_text, sizeof(mode_text), "conference_ai faces:%d analysis:%ux%u", (int)faces.size(), analysis_width, analysis_height);
    cv::putText(frame, mode_text, cv::Point(20, 36), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);

    char track_text[64] = {0};
    snprintf(track_text, sizeof(track_text), "track_lost:%d", lost_frames);
    cv::putText(frame, track_text, cv::Point(20, 72), cv::FONT_HERSHEY_SIMPLEX, 0.75, cv::Scalar(255, 255, 0), 2);
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
        printf("rockx conference detector init success\n");
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

int init_rockx_video_conference_venc_module()
{
    RV1126_VENC_CONFIG conference_venc_config;
    memset(&conference_venc_config, 0, sizeof(conference_venc_config));

    conference_venc_config.id = ROCKX_VIDEO_CONFERENCE_VENC_ID;
    conference_venc_config.attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
    conference_venc_config.attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
    conference_venc_config.attr.stVencAttr.u32PicWidth = 1920;
    conference_venc_config.attr.stVencAttr.u32PicHeight = 1080;
    conference_venc_config.attr.stVencAttr.u32VirWidth = 1920;
    conference_venc_config.attr.stVencAttr.u32VirHeight = 1080;
    conference_venc_config.attr.stVencAttr.u32Profile = 66;

    conference_venc_config.attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    conference_venc_config.attr.stRcAttr.stH264Cbr.u32Gop = 25;
    conference_venc_config.attr.stRcAttr.stH264Cbr.u32BitRate = 1920 * 1080 * 3;
    conference_venc_config.attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
    conference_venc_config.attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 25;
    conference_venc_config.attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    conference_venc_config.attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 25;

    int ret = rkmedia_venc_init(&conference_venc_config);
    if (ret != 0)
    {
        printf("conference venc init error\n");
        return -1;
    }

    RV1126_VENC_CONTAINER conference_venc_container;
    conference_venc_container.id = ROCKX_VIDEO_CONFERENCE_VENC_ID;
    conference_venc_container.venc_id = conference_venc_config.id;
    set_venc_container(conference_venc_container.id, &conference_venc_container);

    printf("conference venc init success\n");
    return 0;
}

void *rockx_video_conference_thread(void *args)
{
    pthread_detach(pthread_self());

    ROCKX_VIDEO_CONFERENCE_PARAM param = *(ROCKX_VIDEO_CONFERENCE_PARAM *)args;
    free(args);

    FACE_TRACK_STATE track_state;
    memset(&track_state, 0, sizeof(track_state));

#if HAVE_ROCKX_SDK
    RockxFaceDetector detector;
    detector.Init();
#else
    printf("rockx sdk header not found, conference thread will forward frames without detection\n");
#endif

    while (1)
    {
        MEDIA_BUFFER src_mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, param.vi_id, -1);
        if (!src_mb)
        {
            printf("conference thread get VI buffer failed\n");
            break;
        }

        MB_IMAGE_INFO_S src_info;
        memset(&src_info, 0, sizeof(src_info));
        if (RK_MPI_MB_GetImageInfo(src_mb, &src_info) != 0)
        {
            printf("conference thread get image info failed\n");
            RK_MPI_MB_ReleaseBuffer(src_mb);
            continue;
        }

        int width = (int)src_info.u32Width;
        int height = (int)src_info.u32Height;
        int hor_stride = (int)(src_info.u32HorStride ? src_info.u32HorStride : src_info.u32Width);
        int ver_stride = (int)(src_info.u32VerStride ? src_info.u32VerStride : src_info.u32Height);

        if (RK_MPI_MB_BeginCPUAccess(src_mb, RK_TRUE) != 0)
        {
            printf("conference thread begin cpu access failed\n");
            RK_MPI_MB_ReleaseBuffer(src_mb);
            continue;
        }

        unsigned char *src_ptr = (unsigned char *)RK_MPI_MB_GetPtr(src_mb);
        std::vector<unsigned char> packed_nv12;
        copy_strided_nv12_to_packed(src_ptr, width, height, hor_stride, ver_stride, packed_nv12);

        std::vector<unsigned char> output_nv12 = packed_nv12;

#if HAVE_OPENCV_SDK
        cv::Mat src_nv12_mat(height * 3 / 2, width, CV_8UC1, packed_nv12.data());
        cv::Mat src_bgr_mat;
        cv::cvtColor(src_nv12_mat, src_bgr_mat, cv::COLOR_YUV2BGR_NV12);

        unsigned int analysis_width = param.analysis_width ? param.analysis_width : (unsigned int)width;
        unsigned int analysis_height = param.analysis_height ? param.analysis_height : (unsigned int)height;

        cv::Mat analysis_bgr_mat;
        if ((int)analysis_width != width || (int)analysis_height != height)
            cv::resize(src_bgr_mat, analysis_bgr_mat, cv::Size((int)analysis_width, (int)analysis_height));
        else
            analysis_bgr_mat = src_bgr_mat;

        std::vector<unsigned char> analysis_nv12;
        bgr_to_nv12(analysis_bgr_mat, analysis_nv12);

        std::vector<FACE_BOX> detected_faces;
#if HAVE_ROCKX_SDK
        detector.Detect(analysis_nv12.data(), (int)analysis_width, (int)analysis_height, detected_faces);
#endif

        std::vector<FACE_BOX> scaled_faces;
        for (size_t i = 0; i < detected_faces.size(); ++i)
            scaled_faces.push_back(scale_face_box(detected_faces[i], (int)analysis_width, (int)analysis_height, width, height));

        FACE_BOX tracked_face = {0, 0, 0, 0, 0.0f};
        bool has_tracked_face = false;

        if (!scaled_faces.empty())
        {
            FACE_BOX selected_face = select_primary_face(scaled_faces, track_state);
            if (track_state.has_box)
                tracked_face = smooth_face_box(track_state.tracked_box, selected_face, param.track_smooth_alpha);
            else
                tracked_face = selected_face;

            track_state.tracked_box = tracked_face;
            track_state.lost_frames = 0;
            track_state.has_box = true;
            has_tracked_face = true;
        }
        else if (track_state.has_box && track_state.lost_frames < (int)param.max_track_lost_frames)
        {
            track_state.lost_frames += 1;
            tracked_face = track_state.tracked_box;
            has_tracked_face = true;
        }
        else
        {
            memset(&track_state, 0, sizeof(track_state));
        }

        if (has_tracked_face)
            apply_face_beauty(src_bgr_mat, tracked_face);

        draw_conference_overlay(src_bgr_mat,
                                scaled_faces,
                                has_tracked_face ? &tracked_face : NULL,
                                analysis_width,
                                analysis_height,
                                track_state.lost_frames);

        bgr_to_nv12(src_bgr_mat, output_nv12);
#else
        if (track_state.has_box && track_state.lost_frames < (int)param.max_track_lost_frames)
            track_state.lost_frames += 1;
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
            printf("conference thread create output buffer failed\n");
            RK_MPI_MB_EndCPUAccess(src_mb, RK_TRUE);
            RK_MPI_MB_ReleaseBuffer(src_mb);
            continue;
        }

        if (RK_MPI_MB_BeginCPUAccess(output_mb, RK_FALSE) != 0)
        {
            printf("conference thread begin output cpu access failed\n");
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
            printf("conference thread send buffer to venc failed\n");

        RK_MPI_MB_ReleaseBuffer(output_mb);
        RK_MPI_MB_EndCPUAccess(src_mb, RK_TRUE);
        RK_MPI_MB_ReleaseBuffer(src_mb);
    }

    return NULL;
}

void *rockx_video_conference_venc_thread(void *args)
{
    pthread_detach(pthread_self());

    ROCKX_VIDEO_CONFERENCE_PARAM param = *(ROCKX_VIDEO_CONFERENCE_PARAM *)args;
    free(args);

    FILE *fp = fopen(param.output_path, "wb");
    if (fp == NULL)
    {
        printf("open conference venc output failed: %s\n", param.output_path);
        return NULL;
    }

    while (1)
    {
        MEDIA_BUFFER mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, param.venc_id, -1);
        if (!mb)
        {
            printf("conference venc thread get buffer failed\n");
            break;
        }

        fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), fp);
        fflush(fp);
        RK_MPI_MB_ReleaseBuffer(mb);
    }

    fclose(fp);
    return NULL;
}
