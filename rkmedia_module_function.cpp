#include "rkmedia_module_function.h"
#include "rkmedia_assignment_manage.h"
#include "rkmedia_config_public.h"
#include "rkmedia_module.h"
#include "rkmedia_container.h"
#include "rockx_video_conference_module.h"
#include "SDL.h"
#include "SDL_ttf.h"
#include <sys/time.h>

#define FILE_IMAGE_LENGTH (64 * 1024)

static int get_align16_value(int input_value, int align)
{
    int handle_value = 0;
    if (align && (input_value % align))
        handle_value = (input_value / align + 1) * align;
    return handle_value;
}

int read_image(char *filename, char *buffer)
{
    if (filename == NULL || buffer == NULL)
        return -1;
    FILE *fp = fopen(filename, "rb"); // 以二进制模式读取该文件
    if (fp == NULL)
    {
        printf("fopen failed\n");
        return -2;
    }

    // 检测文件大小file
    fseek(fp, 0, SEEK_END);
    int length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    int size = fread(buffer, 1, length, fp);
    if (size != length)
    {
        printf("fread failed:%d\n", size);
        return -3;
    }

    fclose(fp);
    return size;
}

static int get_cur_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);                       // 使用gettimeofday获取当前系统时间
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000); // 利用struct timeval结构体将时间转换为ms
}

int init_rkmedia_module_function()
{
    rkmedia_function_init();

    RV1126_VI_CONFIG rkmedia_vi_config;
    memset(&rkmedia_vi_config, 0, sizeof(rkmedia_vi_config));
    rkmedia_vi_config.id = 0;
    rkmedia_vi_config.attr.pcVideoNode = CMOS_DEVICE_NAME;   // VIDEO视频节点路径,
    rkmedia_vi_config.attr.u32BufCnt = 3;                    // VI捕获视频缓冲区计数，默认是3
    rkmedia_vi_config.attr.u32Width = 1920;                  // 视频输入的宽度，一般和CMOS摄像头或者外设的宽度一致
    rkmedia_vi_config.attr.u32Height = 1080;                 // 视频输入的高度，一般和CMOS摄像头或者外设的高度一致
    rkmedia_vi_config.attr.enPixFmt = IMAGE_TYPE_NV12;       // 视频输入的图像格式，默认是NV12(IMAGE_TYPE_NV12)
    rkmedia_vi_config.attr.enBufType = VI_CHN_BUF_TYPE_MMAP; // VI捕捉视频的类型
    rkmedia_vi_config.attr.enWorkMode = VI_WORK_MODE_NORMAL; // VI的工作模式，默认是NORMAL(VI_WORK_MODE_NORMAL)
    int ret = rkmedia_vi_init(&rkmedia_vi_config);           // 初始化VI工作
    if (ret != 0)
    {
        printf("vi init error\n");
    }
    else
    {
        printf("vi init success\n");
        RV1126_VI_CONTAINTER vi_container;
        vi_container.id = 0;
        vi_container.vi_id = rkmedia_vi_config.id;
        set_vi_container(0, &vi_container); // 设置VI容器
    }

    RV1126_VENC_CONFIG rkmedia_venc_config = {0};
    memset(&rkmedia_venc_config, 0, sizeof(rkmedia_venc_config));
    rkmedia_venc_config.id = 0;
    rkmedia_venc_config.attr.stVencAttr.enType = RK_CODEC_TYPE_H264;          // 编码器协议类型
    rkmedia_venc_config.attr.stVencAttr.imageType = IMAGE_TYPE_NV12;          // 输入图像类型
    rkmedia_venc_config.attr.stVencAttr.u32PicWidth = 1920;                   // 编码图像宽度
    rkmedia_venc_config.attr.stVencAttr.u32PicHeight = 1080;                  // 编码图像高度
    rkmedia_venc_config.attr.stVencAttr.u32VirWidth = 1920;                   // 编码图像虚宽度，一般来说u32VirWidth和u32PicWidth是一致的
    rkmedia_venc_config.attr.stVencAttr.u32VirHeight = 1080;                  // 编码图像虚高度，一般来说u32VirHeight和u32PicHeight是一致的
    rkmedia_venc_config.attr.stVencAttr.u32Profile = 66;                      // 编码等级H.264: 66: Baseline; 77:Main Profile; 100:High Profile; H.265: default:Main; Jpege/MJpege: default:Baseline(编码等级的作用主要是改变画面质量，66的画面质量最差利于网络传输，100的质量最好)

    rkmedia_venc_config.attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;        // 编码器码率控制模式
    rkmedia_venc_config.attr.stRcAttr.stH264Cbr.u32Gop = 25;                  // GOPSIZE:关键帧间隔
    rkmedia_venc_config.attr.stRcAttr.stH264Cbr.u32BitRate = 1920 * 1080 * 3; // 码率
    rkmedia_venc_config.attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;      // 目的帧率分子:填的是1固定
    rkmedia_venc_config.attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 25;     // 目的帧率分母:填的是25固定
    rkmedia_venc_config.attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;       // 源头帧率分子:填的是1固定
    rkmedia_venc_config.attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 25;      // 源头帧率分母:填的是25固定

    ret = rkmedia_venc_init(&rkmedia_venc_config);                            // VENC模块的初始化
    if (ret != 0)
    {
        printf("venc init error\n");
    }
    else
    {
        RV1126_VENC_CONTAINER venc_container;
        venc_container.id = 0;
        venc_container.venc_id = rkmedia_venc_config.id;
        set_venc_container(0, &venc_container);
        printf("venc init success\n");
    }

    // RGA
    RGA_ATTR_S rga_info;
    /**Image Input ..............*/
    rga_info.stImgIn.u32Width = 1920;           // 设置RGA输入分辨率宽度
    rga_info.stImgIn.u32Height = 1080;          // 设置RGA输入分辨率高度
    rga_info.stImgIn.u32HorStride = 1920;       // 设置RGA输入分辨率虚宽
    rga_info.stImgIn.u32VirStride = 1080;       // 设置RGA输入分辨率虚高
    rga_info.stImgIn.imgType = IMAGE_TYPE_NV12; // 设置ImageType图像类型
    rga_info.stImgIn.u32X = 0;                  // 设置X坐标
    rga_info.stImgIn.u32Y = 0;                  // 设置Y坐标

    /**Image Output......................*/
    rga_info.stImgOut.u32Width = 1280;           // 设置RGA输出分辨率宽度
    rga_info.stImgOut.u32Height = 720;           // 设置RGA输出分辨率高度
    rga_info.stImgOut.u32HorStride = 1280;       // 设置RGA输出分辨率虚宽
    rga_info.stImgOut.u32VirStride = 720;        // 设置RGA输出分辨率虚高
    rga_info.stImgOut.imgType = IMAGE_TYPE_NV12; // 设置输出ImageType图像类型
    rga_info.stImgOut.u32X = 0;                  // 设置X坐标
    rga_info.stImgOut.u32Y = 0;                  // 设置Y坐标

    // RGA Public Parameter
    rga_info.u16BufPoolCnt = 3; // 缓冲池计数
    rga_info.u16Rotaion = 0;    //
    rga_info.enFlip = RGA_FLIP_H;
    rga_info.bEnBufPool = RK_TRUE;
    ret = RK_MPI_RGA_CreateChn(0, &rga_info);
    if (ret)
    {
        printf("RGA Set Failed.....\n");
    }
    else
    {
        printf("RGA Set Success.....\n");
    }

    RV1126_VENC_CONFIG low_rkmedia_venc_config = {0};
    memset(&low_rkmedia_venc_config, 0, sizeof(low_rkmedia_venc_config));
    low_rkmedia_venc_config.id = 1;
    low_rkmedia_venc_config.attr.stVencAttr.enType = RK_CODEC_TYPE_H264;         // 编码器协议类型
    low_rkmedia_venc_config.attr.stVencAttr.imageType = IMAGE_TYPE_NV12;         // 输入图像类型
    low_rkmedia_venc_config.attr.stVencAttr.u32PicWidth = 1280;                  // 编码图像宽度
    low_rkmedia_venc_config.attr.stVencAttr.u32PicHeight = 720;                  // 编码图像高度
    low_rkmedia_venc_config.attr.stVencAttr.u32VirWidth = 1280;                  // 编码图像虚宽度，一般来说u32VirWidth和u32PicWidth是一致的
    low_rkmedia_venc_config.attr.stVencAttr.u32VirHeight = 720;                  // 编码图像虚高度，一般来说u32VirHeight和u32PicHeight是一致的
    low_rkmedia_venc_config.attr.stVencAttr.u32Profile = 66;                     // 编码等级H.264: 66: Baseline; 77:Main Profile; 100:High Profile; H.265: default:Main; Jpege/MJpege: default:Baseline(编码等级的作用主要是改变画面质量，66的画面质量最差利于网络传输，100的质量最好)

    low_rkmedia_venc_config.attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;       // 编码器码率控制模式
    low_rkmedia_venc_config.attr.stRcAttr.stH264Cbr.u32Gop = 30;                 // GOPSIZE:关键帧间隔
    low_rkmedia_venc_config.attr.stRcAttr.stH264Cbr.u32BitRate = 1280 * 720 * 3; // 码率
    low_rkmedia_venc_config.attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;     // 目的帧率分子:填的是1固定
    low_rkmedia_venc_config.attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 25;    // 目的帧率分母:填的是25固定
    low_rkmedia_venc_config.attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;      // 源头帧率分子:填的是1固定
    low_rkmedia_venc_config.attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 25;     // 源头帧率分母:填的是25固定    
    ret = rkmedia_venc_init(&low_rkmedia_venc_config);                           // VENC模块的初始化
    if (ret != 0)
    {
        printf("venc init error\n");
    }
    else
    {
        RV1126_VENC_CONTAINER low_venc_container;
        low_venc_container.id = 1;
        low_venc_container.venc_id = low_rkmedia_venc_config.id;
        set_venc_container(low_venc_container.id, &low_venc_container);
        printf("low_venc init success\n");
    }

    ret = init_rockx_video_conference_venc_module();
    if (ret != 0)
    {
        printf("conference venc module init error\n");
    }

    /*ret = RK_MPI_VENC_RGN_Init(0, NULL); 
    if (ret)
    {
        printf("Create HIGH_VENC_RGN Failed .....\n");
        return 0;
    }
    else
    {
        printf("Create HIGI_VENC_RGN Success .....\n");
    }

    int t1;
    t1 = get_cur_time_ms();
    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);

    char time_str[30];
    sprintf(time_str, "%04d-%02d-%02d %02d:%02d:%02d",
            tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
            tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    printf("sizeof_time_str = %d\n", strlen(time_str));

    char * time_string = "2019-11-21 15:40:29";

    TTF_Font *ttf_font;
    char pstr[128] = {0};
    snprintf(pstr, 128, "%s", "HelloWorld");

    SDL_Surface *text_surface;
    SDL_Surface *convert_text_surface;
    SDL_PixelFormat *pixel_format;*/

#if 0
    // TTF模块的初始化
#if 0
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
    text_surface = TTF_RenderUTF8_Solid(ttf_font, time_string, sdl_color); ////渲染文字

    // ARGB_8888
    pixel_format = (SDL_PixelFormat *)malloc(sizeof(SDL_PixelFormat));
    pixel_format->BitsPerPixel = 32;  // 每个像素所占的比特位数
    pixel_format->BytesPerPixel = 4;  // 每个像素所占的字节数
    pixel_format->Amask = 0XFF000000; // ARGB的A掩码，A位0xff
    pixel_format->Rmask = 0X00FF0000; // ARGB的R掩码，R位0xff
    pixel_format->Gmask = 0X0000FF00; // ARGB的G掩码，G位0xff
    pixel_format->Bmask = 0X000000FF; // ARGB的B掩码，B位0xff
    pixel_format->colorkey = 0x000000ff;
    pixel_format->alpha = 0xff;

    convert_text_surface = SDL_ConvertSurface(text_surface, pixel_format, 0);
    if (convert_text_surface == NULL)
    {
        printf("convert_text_surface failed...\n");
    }

    BITMAP_S bitmap;
    bitmap.u32Width = get_align16_value(convert_text_surface->w, 16);  // Bitmap的宽度
    bitmap.u32Height = get_align16_value(convert_text_surface->h, 16); // Bitmap的高度
    bitmap.enPixelFormat = PIXEL_FORMAT_ARGB_8888;
    bitmap.pData = malloc((bitmap.u32Width) * (bitmap.u32Height) * pixel_format->BytesPerPixel);                                             ////bitmap的data的分配大小
    memcpy(bitmap.pData, convert_text_surface->pixels, (convert_text_surface->w) * (convert_text_surface->h) * pixel_format->BytesPerPixel); ////bitmap的data赋值
#endif



#if 0
    printf("ssssss\n");
    SDL_Surface* load_picture = SDL_LoadBMP("./0.bmp");    
    printf("gggggggg\n");                                                                                                         BITMAP_S bitmap;
    bitmap.u32Width = get_align16_value(load_picture->w, 16);                                                                        // Bitmap的宽度
    bitmap.u32Height = get_align16_value(load_picture->h, 16);                                                                    // Bitmap的高度
    bitmap.enPixelFormat = PIXEL_FORMAT_ARGB_8888;                                                                                           
    bitmap.pData = malloc((bitmap.u32Width) * (bitmap.u32Height) *4);                                             
    memcpy(bitmap.pData, load_picture->pixels, (load_picture->w) * (load_picture->h) * 4);
    printf("dddddddd\n");
#endif

    OSD_REGION_INFO_S rgn_info;            // OSD_RGN_INFO结构体
    rgn_info.enRegionId = REGION_ID_0;     // rgn的区域ID
    rgn_info.u32Width = bitmap.u32Width;   // osd的长度
    rgn_info.u32Height = bitmap.u32Height; // osd的高度

    //rgn_info.u32Width = 560;   // osd的长度
    //rgn_info.u32Height = 560; // osd的高度

    rgn_info.u32PosX = 128; // Osd的X轴方向
    rgn_info.u32PosY = 128; // Osd的Y轴方向
    rgn_info.u8Enable = 1;  ////使能OSD模块，1是使能，0为禁止。
    rgn_info.u8Inverse = 0; // 禁止翻转

    ret = RK_MPI_VENC_RGN_SetBitMap(0, &rgn_info, &bitmap); // 设置OSD位图
    if (ret)
    {
        printf("HIGI_RK_MPI_VENC_RGN_SetBitMap failed...\n");
    }
    else
    {
        printf("HIGI_RK_MPI_VENC_RGN_SetBitMap Success...\n");
    }

    ret = RK_MPI_RGA_RGN_SetBitMap(0, &rgn_info, &bitmap);
    if (ret)
    {
        printf("RK_MPI_RGA_RGN_SetBitMap failed...\n");
    }
    else
    {
        printf("RK_MPI_RGA_RGN_SetBitMap Success...\n");
    }
#endif

    return 0;
}
