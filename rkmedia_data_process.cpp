#include "rkmedia_data_process.h"
#include "ffmpeg_video_queue.h"
#include "ffmpeg_audio_queue.h"
#include "rkmedia_module.h"
#include "rkmedia_ffmpeg_config.h"
#include "rkmedia_data_process.h"
#include "SDL.h"
#include "SDL_ttf.h"

extern VIDEO_QUEUE *high_video_queue;
extern VIDEO_QUEUE *low_video_queue;

static int get_align16_value(int input_value, int align)
{
    int handle_value = 0;
    if (align && (input_value % align))
        handle_value = (input_value / align + 1) * align;
    return handle_value;
}

// 从RV1126视频编码数据赋值到FFMPEG的Video AVPacket中
AVPacket *get_high_ffmpeg_video_avpacket(AVPacket *pkt)
{
    video_data_packet_t *video_data_packet = high_video_queue->getVideoPacketQueue(); // 从视频队列获取数据

    if (video_data_packet != NULL)
    {
        /*
     重新分配给定的缓冲区
   1.  如果入参的 AVBufferRef 为空，直接调用 av_realloc 分配一个新的缓存区，并调用 av_buffer_create 返回一个新的 AVBufferRef 结构；
   2.  如果入参的缓存区长度和入参 size 相等，直接返回 0；
   3.  如果对应的 AVBuffer 设置了 BUFFER_FLAG_REALLOCATABLE 标志，或者不可写，再或者 AVBufferRef data 字段指向的数据地址和 AVBuffer 的 data 地址不同，递归调用 av_buffer_realloc 分配一个新
的 buffer，并将 data 拷贝过去；
   4.  不满足上面的条件，直接调用 av_realloc 重新分配缓存区。
 */
        int ret = av_buffer_realloc(&pkt->buf, video_data_packet->video_frame_size + 70);
        if (ret < 0)
        {
            return NULL;
        }
        pkt->size = video_data_packet->video_frame_size;                                        // rv1126的视频长度赋值到AVPacket Size
        memcpy(pkt->buf->data, video_data_packet->buffer, video_data_packet->video_frame_size); // rv1126的视频数据赋值到AVPacket data
        pkt->data = pkt->buf->data;                                                             // 把pkt->buf->data赋值到pkt->data
        pkt->flags |= AV_PKT_FLAG_KEY;                                                          // 默认flags是AV_PKT_FLAG_KEY
        if (video_data_packet != NULL)
        {
            free(video_data_packet);
            video_data_packet = NULL;
        }

        return pkt;
    }
    else
    {
        return NULL;
    }
}


AVPacket *get_low_ffmpeg_video_avpacket(AVPacket *pkt)
{
    video_data_packet_t *video_data_packet = low_video_queue->getVideoPacketQueue(); // 从视频队列获取数据

    if (video_data_packet != NULL)
    {
        /*
     重新分配给定的缓冲区
   1.  如果入参的 AVBufferRef 为空，直接调用 av_realloc 分配一个新的缓存区，并调用 av_buffer_create 返回一个新的 AVBufferRef 结构；
   2.  如果入参的缓存区长度和入参 size 相等，直接返回 0；
   3.  如果对应的 AVBuffer 设置了 BUFFER_FLAG_REALLOCATABLE 标志，或者不可写，再或者 AVBufferRef data 字段指向的数据地址和 AVBuffer 的 data 地址不同，递归调用 av_buffer_realloc 分配一个新
的 buffer，并将 data 拷贝过去；
   4.  不满足上面的条件，直接调用 av_realloc 重新分配缓存区。
 */
        int ret = av_buffer_realloc(&pkt->buf, video_data_packet->video_frame_size + 70);
        if (ret < 0)
        {
            return NULL;
        }
        pkt->size = video_data_packet->video_frame_size;                                        // rv1126的视频长度赋值到AVPacket Size
        memcpy(pkt->buf->data, video_data_packet->buffer, video_data_packet->video_frame_size); // rv1126的视频数据赋值到AVPacket data
        pkt->data = pkt->buf->data;                                                             // 把pkt->buf->data赋值到pkt->data
        pkt->flags |= AV_PKT_FLAG_KEY;                                                          // 默认flags是AV_PKT_FLAG_KEY
        if (video_data_packet != NULL)
        {
            free(video_data_packet);
            video_data_packet = NULL;
        }

        return pkt;
    }
    else
    {
        return NULL;
    }
}



int write_ffmpeg_avpacket(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /*将输出数据包时间戳值从编解码器重新调整为流时基 */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;

    return av_interleaved_write_frame(fmt_ctx, pkt);
}

int deal_high_video_avpacket(AVFormatContext *oc, OutputStream *ost)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVPacket *video_packet = get_high_ffmpeg_video_avpacket(ost->packet); // 从RV1126视频编码数据赋值到FFMPEG的Video AVPacket中
    if (video_packet != NULL)
    {
        video_packet->pts = ost->next_timestamp++; // VIDEO_PTS按照帧率进行累加
    }

    ret = write_ffmpeg_avpacket(oc, &c->time_base, ost->stream, video_packet); // 向复合流写入视频数据
    if (ret != 0)
    {
        printf("write video avpacket error");
        return -1;
    }

    return 0;
}

int deal_low_video_avpacket(AVFormatContext *oc, OutputStream *ost)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVPacket *video_packet = get_low_ffmpeg_video_avpacket(ost->packet); // 从RV1126视频编码数据赋值到FFMPEG的Video AVPacket中
    if (video_packet != NULL)
    {
        video_packet->pts = ost->next_timestamp++; // VIDEO_PTS按照帧率进行累加
    }

    ret = write_ffmpeg_avpacket(oc, &c->time_base, ost->stream, video_packet); // 向复合流写入视频数据
    if (ret != 0)
    {
        printf("write video avpacket error");
        return -1;
    }

    return 0;
}

#if 0
void *osd_venc_thread(void *args)
{
    pthread_detach(pthread_self());
    int ret;
    TTF_Font *ttf_font;
    char *pstr = "2019-11-21 15:40:29";
    SDL_Surface *text_surface;
    SDL_Surface *convert_text_surface;
    SDL_PixelFormat *pixel_format;

    // TTF模块的初始化
    ret = TTF_Init();
    if (ret < 0)
    {
        printf("TTF_Init Failed...\n");
    }

    // 打开TTF的字库
    ttf_font = TTF_OpenFont("./fzlth.ttf", 48);
    if (ttf_font == NULL)
    {
        printf("TTF_OpenFont Failed...\n");
    }

    // SDL_COLOR黑色,RGB(0,0,0)
    SDL_Color sdl_color;
    sdl_color.r = 0;
    sdl_color.g = 0;
    sdl_color.b = 0;
    text_surface = TTF_RenderText_Solid(ttf_font, pstr, sdl_color); ////渲染文字

    // ARGB_8888
    pixel_format = (SDL_PixelFormat *)malloc(sizeof(SDL_PixelFormat));
    pixel_format->BitsPerPixel = 32;  // 每个像素所占的比特位数
    pixel_format->BytesPerPixel = 4;  // 每个像素所占的字节数
    pixel_format->Amask = 0XFF000000; // ARGB的A掩码，A位0xff
    pixel_format->Rmask = 0X00FF0000; // ARGB的R掩码，R位0xff
    pixel_format->Gmask = 0X0000FF00; // ARGB的G掩码，G位0xff
    pixel_format->Bmask = 0X000000FF; // ARGB的B掩码，B位0xff
    convert_text_surface = SDL_ConvertSurface(text_surface, pixel_format, 0);
    if (convert_text_surface == NULL)
    {
        printf("convert_text_surface failed...\n");
    }

    BITMAP_S bitmap;                                                                                                                         // Bitmap位图结构体
    bitmap.u32Width = get_align16_value(convert_text_surface->w, 16);                                                                        // Bitmap的宽度
    bitmap.u32Height = get_align16_value(convert_text_surface->h, 16);                                                                       // Bitmap的高度
    bitmap.enPixelFormat = PIXEL_FORMAT_ARGB_8888;                                                                                           ////像素格式ARGB8888
    bitmap.pData = malloc((bitmap.u32Width) * (bitmap.u32Height) * pixel_format->BytesPerPixel);                                             ////bitmap的data的分配大小
    memcpy(bitmap.pData, convert_text_surface->pixels, (convert_text_surface->w) * (convert_text_surface->h) * pixel_format->BytesPerPixel); ////bitmap的data赋值

    OSD_REGION_INFO_S rgn_info;            // OSD_RGN_INFO结构体
    rgn_info.enRegionId = REGION_ID_0;     // rgn的区域ID
    rgn_info.u32Width = bitmap.u32Width;   // osd的长度
    rgn_info.u32Height = bitmap.u32Height; // osd的高度
    rgn_info.u32PosX = 128;                // Osd的X轴方向
    rgn_info.u32PosY = 128;                // Osd的Y轴方向
    rgn_info.u8Enable = 1;                 ////使能OSD模块，1是使能，0为禁止。
    rgn_info.u8Inverse = 0;                // 禁止翻转

    ret = RK_MPI_VENC_RGN_SetBitMap(0, &rgn_info, &bitmap); // 设置OSD位图
    if (ret)
    {
        printf("HIGI_RK_MPI_VENC_RGN_SetBitMap failed...\n");
    }
    else
    {
        printf("HIGI_RK_MPI_VENC_RGN_SetBitMap Success...\n");
    }

    return NULL;
}


void *low_osd_venc_thread(void *args)
{
    pthread_detach(pthread_self());
    int ret;
    TTF_Font *ttf_font;
    char *pstr = "2019-11-21 15:40:29";
    SDL_Surface *text_surface;
    SDL_Surface *convert_text_surface;
    SDL_PixelFormat *pixel_format;

    // TTF模块的初始化
    ret = TTF_Init();
    if (ret < 0)
    {
        printf("TTF_Init Failed...\n");
    }

    // 打开TTF的字库
    ttf_font = TTF_OpenFont("./fzlth.ttf", 48);
    if (ttf_font == NULL)
    {
        printf("TTF_OpenFont Failed...\n");
    }

    // SDL_COLOR黑色,RGB(0,0,0)
    SDL_Color sdl_color;
    sdl_color.r = 0;
    sdl_color.g = 0;
    sdl_color.b = 0;
    text_surface = TTF_RenderText_Solid(ttf_font, pstr, sdl_color); ////渲染文字

    // ARGB_8888
    pixel_format = (SDL_PixelFormat *)malloc(sizeof(SDL_PixelFormat));
    pixel_format->BitsPerPixel = 32;  // 每个像素所占的比特位数
    pixel_format->BytesPerPixel = 4;  // 每个像素所占的字节数
    pixel_format->Amask = 0XFF000000; // ARGB的A掩码，A位0xff
    pixel_format->Rmask = 0X00FF0000; // ARGB的R掩码，R位0xff
    pixel_format->Gmask = 0X0000FF00; // ARGB的G掩码，G位0xff
    pixel_format->Bmask = 0X000000FF; // ARGB的B掩码，B位0xff
    convert_text_surface = SDL_ConvertSurface(text_surface, pixel_format, 0);
    if (convert_text_surface == NULL)
    {
        printf("convert_text_surface failed...\n");
    }

    BITMAP_S bitmap;                                                                                                                         // Bitmap位图结构体
    bitmap.u32Width = get_align16_value(convert_text_surface->w, 16);                                                                        // Bitmap的宽度
    bitmap.u32Height = get_align16_value(convert_text_surface->h, 16);                                                                       // Bitmap的高度
    bitmap.enPixelFormat = PIXEL_FORMAT_ARGB_8888;                                                                                           ////像素格式ARGB8888
    bitmap.pData = malloc((bitmap.u32Width) * (bitmap.u32Height) * pixel_format->BytesPerPixel);                                             ////bitmap的data的分配大小
    memcpy(bitmap.pData, convert_text_surface->pixels, (convert_text_surface->w) * (convert_text_surface->h) * pixel_format->BytesPerPixel); ////bitmap的data赋值

    OSD_REGION_INFO_S rgn_info;            // OSD_RGN_INFO结构体
    rgn_info.enRegionId = REGION_ID_0;     // rgn的区域ID
    rgn_info.u32Width = bitmap.u32Width;   // osd的长度
    rgn_info.u32Height = bitmap.u32Height; // osd的高度
    rgn_info.u32PosX = 256;                // Osd的X轴方向
    rgn_info.u32PosY = 256;                // Osd的Y轴方向
    rgn_info.u8Enable = 1;                 ////使能OSD模块，1是使能，0为禁止。
    rgn_info.u8Inverse = 0;                // 禁止翻转

    ret = RK_MPI_VENC_RGN_SetBitMap(0, &rgn_info, &bitmap); // 设置OSD位图
    if (ret)
    {
        printf("HIGI_RK_MPI_VENC_RGN_SetBitMap failed...\n");
    }
    else
    {
        printf("HIGI_RK_MPI_VENC_RGN_SetBitMap Success...\n");
    }

    ret =  RK_MPI_RGA_RGN_SetBitMap(0, &rgn_info, &bitmap);
    if (ret)
    {
        printf("RK_MPI_RGA_RGN_SetBitMap failed...\n");
    }
    else
    {
        printf("RK_MPI_RGA_RGN_SetBitMap Success...\n");
    }

    return NULL;
}
#endif


void *camera_venc_thread(void *args)
{
    pthread_detach(pthread_self());
    MEDIA_BUFFER mb = NULL;

    VENC_PROC_PARAM venc_arg = *(VENC_PROC_PARAM *)args;
    free(args);

    printf("video_venc_thread...\n");

    while (1)
    {
        // 从指定通道中获取VENC数据
        mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, venc_arg.vencId, -1);
        if (!mb)
        {
            printf("high_get venc media buffer error\n");
            break;
        }

        // int naluType = RK_MPI_MB_GetFlag(mb);
        // 分配video_data_packet_t结构体
        video_data_packet_t *video_data_packet = (video_data_packet_t *)malloc(sizeof(video_data_packet_t));
        // 把VENC视频缓冲区数据传输到video_data_packet的buffer中
        memcpy(video_data_packet->buffer, RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetSize(mb));
        // 把VENC的长度赋值给video_data_packet的video_frame_size中
        video_data_packet->video_frame_size = RK_MPI_MB_GetSize(mb);
        // video_data_packet->frame_flag = naluType;
        // 入到视频压缩队列
        high_video_queue->putVideoPacketQueue(video_data_packet);
        // printf("#naluType = %d \n", naluType);
        // 释放VENC资源
        RK_MPI_MB_ReleaseBuffer(mb);
    }

    MPP_CHN_S vi_channel;
    MPP_CHN_S venc_channel;

    vi_channel.enModId = RK_ID_VI;
    vi_channel.s32ChnId = 0;

    venc_channel.enModId = RK_ID_VENC;
    venc_channel.s32ChnId = venc_arg.vencId;

    int ret;
    ret = RK_MPI_SYS_UnBind(&vi_channel, &venc_channel);
    if (ret != 0)
    {
        printf("VI UnBind failed \n");
    }
    else
    {
        printf("Vi UnBind success\n");
    }

    ret = RK_MPI_VENC_DestroyChn(0);
    if (ret)
    {
        printf("Destroy Venc error! ret=%d\n", ret);
        return 0;
    }
    // destroy vi
    ret = RK_MPI_VI_DisableChn(0, 0);
    if (ret)
    {
        printf("Disable Chn Venc error! ret=%d\n", ret);
        return 0;
    }

    return NULL;
}

void * get_rga_thread(void * args)
{
    MEDIA_BUFFER mb = NULL;

    while (1)
    {
        mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_RGA, 0 , -1);  //获取RGA的数据
        if(!mb)
        {
            break;
        }

        RK_MPI_SYS_SendMediaBuffer(RK_ID_VENC, 1, mb); //
        RK_MPI_MB_ReleaseBuffer(mb);
    }

    return NULL;
}


void *low_camera_venc_thread(void *args)
{
    pthread_detach(pthread_self());
    MEDIA_BUFFER mb = NULL;

    VENC_PROC_PARAM venc_arg = *(VENC_PROC_PARAM *)args;
    free(args);

    printf("low_video_venc_thread...\n");

    while (1)
    {
        // 从指定通道中获取VENC数据
        //mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, venc_arg.vencId, -1);
        mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, 1, -1);
        if (!mb)
        {
            printf("low_venc break....\n");
            break;
        }

        // int naluType = RK_MPI_MB_GetFlag(mb);
        // 分配video_data_packet_t结构体
        video_data_packet_t *video_data_packet = (video_data_packet_t *)malloc(sizeof(video_data_packet_t));
        // 把VENC视频缓冲区数据传输到video_data_packet的buffer中
        memcpy(video_data_packet->buffer, RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetSize(mb));
        // 把VENC的长度赋值给video_data_packet的video_frame_size中
        video_data_packet->video_frame_size = RK_MPI_MB_GetSize(mb);
        // video_data_packet->frame_flag = naluType;
        // 入到视频压缩队列
        low_video_queue->putVideoPacketQueue(video_data_packet);
        // printf("#naluType = %d \n", naluType);
        // 释放VENC资源
        RK_MPI_MB_ReleaseBuffer(mb);
    }

    return NULL;
}

// 音视频合成推流线程
void *high_video_push_thread(void *args)
{
    pthread_detach(pthread_self());
    RKMEDIA_FFMPEG_CONFIG ffmpeg_config = *(RKMEDIA_FFMPEG_CONFIG *)args;
    free(args);
    AVOutputFormat *fmt = NULL;
    int ret;

    while (1)
    {
        ret = deal_high_video_avpacket(ffmpeg_config.oc, &ffmpeg_config.video_stream); // 处理FFMPEG视频数据
        if (ret == -1)
        {
            printf("deal_video_avpacket error\n");
            break;
        }
    }

    av_write_trailer(ffmpeg_config.oc);                         // 写入AVFormatContext的尾巴
    free_stream(ffmpeg_config.oc, &ffmpeg_config.video_stream); // 释放VIDEO_STREAM的资源
    free_stream(ffmpeg_config.oc, &ffmpeg_config.audio_stream); // 释放AUDIO_STREAM的资源
    avio_closep(&ffmpeg_config.oc->pb);                         // 释放AVIO资源
    avformat_free_context(ffmpeg_config.oc);                    // 释放AVFormatContext资源
    return NULL;
}

void *low_video_push_thread(void *args)
{
    pthread_detach(pthread_self());
    RKMEDIA_FFMPEG_CONFIG ffmpeg_config = *(RKMEDIA_FFMPEG_CONFIG *)args;
    free(args);
    AVOutputFormat *fmt = NULL;
    int ret;

    while (1)
    {
        ret = deal_low_video_avpacket(ffmpeg_config.oc, &ffmpeg_config.video_stream); // 处理FFMPEG视频数据
        if (ret == -1)
        {
            printf("deal_video_avpacket error\n");
            break;
        }
    }

    av_write_trailer(ffmpeg_config.oc);                         // 写入AVFormatContext的尾巴
    free_stream(ffmpeg_config.oc, &ffmpeg_config.video_stream); // 释放VIDEO_STREAM的资源
    free_stream(ffmpeg_config.oc, &ffmpeg_config.audio_stream); // 释放AUDIO_STREAM的资源
    avio_closep(&ffmpeg_config.oc->pb);                         // 释放AVIO资源
    avformat_free_context(ffmpeg_config.oc);                    // 释放AVFormatContext资源
    return NULL;
}
