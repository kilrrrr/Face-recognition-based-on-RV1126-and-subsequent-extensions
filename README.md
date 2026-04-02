# rv1126_ffmpeg_network_project

基于 Rockchip RV1126 的嵌入式音视频前端工程，核心目标是在板端完成 `视频采集 -> 硬件编码 -> FFmpeg 封装 -> 网络推流`，并在稳定主链路上扩展 `RockX + OpenCV` 人脸检测旁路。

当前工程已经形成三条主要视频支路：

- 主码流：`VI -> VENC(1080P H.264) -> FFmpeg -> 网络输出`
- 子码流：`VI -> RGA(缩放/翻转) -> VENC(720P H.264) -> FFmpeg -> 网络输出`
- 智能分析支路：`VI -> RockX 检测 -> OpenCV 画框 -> VENC -> 本地 H.264 输出`

这个项目更适合被理解为一套“嵌入式音视频前端 + 智能分析扩展”的工程，而不是单独的推流 demo 或单独的人脸检测 demo。

## 项目特点

- 基于 `RKMedia / Rockchip MPP / RKAIQ / FFmpeg` 构建 RV1126 板端视频主链路
- 支持双码流输出，兼顾高质量业务流和低带宽预览流
- 采用 `VENC -> 队列 -> FFmpeg` 的异步解耦方式，降低编码侧与网络发送侧耦合
- 新增 `RockX + OpenCV` 人脸检测旁路，不污染原主码流
- 预留 `AI / AENC` 音频采集与编码接口，具备继续补齐音视频闭环的基础
- 仓库内附带多份代码说明文档，便于继续阅读和扩展

## 整体架构

```text
                +-------------------+
                |   Camera / VI     |
                +---------+---------+
                          |
          +---------------+----------------+
          |                                |
          v                                v
  +---------------+                +---------------+
  |   VENC(0)     |                |     RGA       |
  | 1080P H.264   |                | 1080P -> 720P |
  +-------+-------+                +-------+-------+
          |                                |
          v                                v
  +---------------+                +---------------+
  | high queue    |                |   VENC(1)     |
  +-------+-------+                |  720P H.264   |
          |                        +-------+-------+
          v                                |
  +---------------+                        v
  |   FFmpeg      |                +---------------+
  | high push     |                | low queue     |
  +---------------+                +-------+-------+
                                            |
                                            v
                                     +---------------+
                                     |   FFmpeg      |
                                     | low push      |
                                     +---------------+

                          +---------------------------+
                          | RockX + OpenCV side path |
                          +-------------+-------------+
                                        |
                                        v
                             VI -> detect -> draw box
                                        |
                                        v
                                   VENC(2) H.264
                                        |
                                        v
                           ./rockx_face_detect_venc.h264
```

## 核心设计思路

### 1. 双码流不是重复功能，而是前端能力分层

- 主码流保留更高质量的视频，用于正式推流、录像、回放或后续复核
- 子码流承担弱网预览、调试和轻量分发任务
- 不同业务不强压在同一路编码链路上，便于在资源受限的板端做取舍

### 2. 编码和推流解耦，优先保证实时采集链路稳定

- 编码线程持续从 `VENC` 拉取码流
- 推流线程从线程安全队列中取数据并交给 FFmpeg
- 这样可以避免网络抖动直接反压到硬件编码侧

### 3. 智能分析用旁路扩展，而不是直接改坏主链路

- 人脸检测支路独立编码输出，默认保存为 `./rockx_face_detect_venc.h264`
- 原始主码流保持纯净，便于调试、回放和问题定位
- 算法耗时波动不会直接污染主业务输出

## 代码入口

- 主入口：`rv1126_ffmpeg_main.cpp`
- RKMedia 模块初始化：`rkmedia_module_function.cpp`
- 模块绑定与线程启动：`rkmedia_assignment_manage.cpp`
- 码流读取、RGA 转发、FFmpeg 推流：`rkmedia_data_process.cpp`
- FFmpeg 输出上下文：`rkmedia_ffmpeg_config.cpp`
- RockX + OpenCV 人脸检测支路：`rockx_face_detect_module.cpp`
- ISP 初始化封装：`rv1126_isp_function.cpp`、`sample_common_isp.c`

## 目录说明

### 工程源码

- `rv1126_ffmpeg_main.cpp`：程序入口，负责读取参数和启动整个系统
- `rkmedia_module_function.*`：创建 `VI / VENC / RGA` 等核心模块
- `rkmedia_assignment_manage.*`：完成 `VI -> VENC`、`VI -> RGA` 绑定并启动线程
- `rkmedia_data_process.*`：从 `VENC` 取码流、写入队列、交给 FFmpeg 推流
- `rkmedia_ffmpeg_config.*`：创建 FFmpeg 输出上下文和流配置
- `rockx_face_detect_module.*`：RockX 检测、OpenCV 画框、检测结果码流保存
- `rv1126_isp_function.*`、`sample_common_isp.c`：ISP/RKAIQ 能力封装

### 依赖与预编译资源

- `include/`：RKMedia、RKAIQ 等头文件
- `rv1126_lib/`：板端相关动态库与模型文件
- `opt/arm32_ffmpeg_srt/`：交叉编译后的 FFmpeg/SRT 资源
- `opt/arm_opencv/`：OpenCV 交叉编译资源
- `arm_sdl/`、`arm_sdl_ttf_install/`、`arm_freetype/`：OSD/字体实验依赖

### 文档

- `project_code_summary.md`：项目代码总览
- `rkmedia_module_function_summary.md`：RKMedia 初始化链路总结
- `rockx_opencv_face_detect_summary.md`：人脸检测支路说明
- `camera_expansion_roadmap.md`：camera/ISP 后续拓展路线
- `project_resume.md`：面向简历表达的项目整理

## 依赖环境

本项目面向 `RV1126 Linux SDK` 交叉编译环境，默认不是在 Windows 本机直接编译运行。

主要依赖包括：

- Rockchip RV1126/RV1109 Linux SDK
- RKMedia / Rockchip MPP / RGA
- RKAIQ ISP 相关库
- FFmpeg
- OpenCV
- RockX
- SDL / SDL_ttf / FreeType（主要用于历史 OSD 实验代码）
- x264 / SRT / OpenSSL 等第三方库

## 编译说明

当前仓库保留了一份面向 RV1126 交叉编译环境的 `Makefile`。

### 1. 检查交叉编译器路径

`Makefile` 中默认使用：

```makefile
/opt/rv1126_rv1109_linux_sdk_v1.8.0_20210224/.../arm-linux-gnueabihf-g++
```

如果你的 SDK 安装位置不同，需要先修改对应路径。

### 2. 检查头文件和库目录

重点确认这些目录是否和你的环境一致：

- `./rv1126_lib`
- `./opt/arm32_ffmpeg_srt`
- `./opt/arm_opencv`
- `./opt/arm_libsrt`
- `./opt/arm_openssl`
- `./arm_sdl`
- `./arm_freetype`
- `./arm_sdl_ttf_install`
- `./rknn_rockx_include`
- `./im2d_api`

其中后两个目录在不同开发环境里通常需要按本机 SDK 重新对齐。

### 3. 编译

```bash
make
```

编译成功后会生成：

```bash
./rv1126_ffmpeg_main
```

## 运行方式

程序当前需要 4 个参数：

```bash
./rv1126_ffmpeg_main high_stream_type high_url_address low_stream_type low_url_address
```

参数说明：

- `high_stream_type`：主码流输出类型
- `high_url_address`：主码流目标地址
- `low_stream_type`：子码流输出类型
- `low_url_address`：子码流目标地址

代码里定义了两种类型：

- `0`：`FLV_PROTOCOL`
- `1`：`TS_PROTOCOL`

示例：

```bash
./rv1126_ffmpeg_main 0 rtmp://192.168.1.10/live/high 0 rtmp://192.168.1.10/live/low
```

说明：

- 当前工程主链路主要围绕 `RTMP/FLV` 使用场景整理
- `TS_PROTOCOL` 在代码结构中已保留，但你仍需结合自己的 FFmpeg/SRT 运行环境验证地址格式和可用性

## 当前输出

### 网络输出

- 主码流：1080P H.264，经 FFmpeg 封装后输出到指定网络地址
- 子码流：720P H.264，经 FFmpeg 封装后输出到指定网络地址

### 本地输出

- 人脸检测结果码流默认保存到：

```bash
./rockx_face_detect_venc.h264
```

## 当前项目状态

### 已完成

- `VI -> VENC(0)` 主码流链路
- `VI -> RGA -> VENC(1)` 子码流链路
- `VENC -> 队列 -> FFmpeg` 异步推流结构
- RockX + OpenCV 人脸检测旁路
- ISP 相关初始化接口封装

### 已预留但仍可继续完善

- `AI / AENC` 音频链路
- 音视频同步与复用
- OSD/字体叠加能力
- 更完整的 ISP 参数控制接入
- Camera 管理层抽象

## 适用场景

- RV1126 智能摄像头前端
- 嵌入式视频采集与推流终端
- 边缘侧视频分析前端
- 智能安防/门禁/巡检类前端原型

## 后续规划

这个项目后续最值得继续推进的方向主要有：

- 把 ISP 控制能力真正接入主流程，形成可调成像链路
- 抽出 `Camera Manager`，统一管理 sensor、VI、ISP、stream 和控制接口
- 将多路流整理为预览流、业务流、算法流三层模型
- 补齐 `AI -> AENC -> FFmpeg`，形成完整音视频闭环
- 为视频帧补充时间戳、曝光值、算法结果等元数据
- 在 Linux 路线下先完成 camera 前端能力；只有在 Android 化时再考虑 `Camera HAL`

## 相关文档

- [项目代码总览](./project_code_summary.md)
- [RKMedia 初始化总结](./rkmedia_module_function_summary.md)
- [RockX + OpenCV 人脸检测说明](./rockx_opencv_face_detect_summary.md)
- [Camera 拓展路线](./camera_expansion_roadmap.md)
- [项目简历整理](./project_resume.md)

## 注意事项

- 本项目依赖较多板端库和交叉编译资源，移植到新环境时优先检查 `Makefile`
- 仓库中的部分第三方资源是为了便于开发环境复现，实际部署时建议按目标板 SDK 重新梳理
- 目前代码主干仍以视频链路为主，音频能力更多是结构预留
- RockX/OpenCV 支路已经接入，但在不同 SDK 版本下头文件路径和接口细节可能需要小幅适配
