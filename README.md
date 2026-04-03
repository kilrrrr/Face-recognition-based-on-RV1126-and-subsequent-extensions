# 基于 RV1126 的嵌入式音视频前端与会议 AI 分析 Demo

## 项目简介

本项目基于 Rockchip RV1126 平台实现一套板端嵌入式音视频前端，核心目标是打通：

- 视频采集
- 图像缩放
- 硬件编码
- FFmpeg 网络推流

在原有双码流视频链路基础上，项目进一步增加了一个偏直播 / 视频会议方向的 AI 分析旁路，基于 `RockX + OpenCV` 实现：

- 低分辨率人脸检测
- 主目标稳定跟踪
- 人脸 ROI 轻量美颜
- 独立 AI 结果视频输出

因此，这个仓库更适合被理解为：

`嵌入式音视频前端 + 会议 AI 分析扩展`

而不只是单纯的视频推流 Demo。

## 当前三条视频支路

### 1. 主码流

```text
VI -> VENC(0) -> 队列 -> FFmpeg -> 网络输出
```

- 分辨率：`1920 x 1080`
- 作用：
  - 高质量主视频输出
  - 录像 / 业务流 / 回放复核

### 2. 预览码流

```text
VI -> RGA -> VENC(1) -> 队列 -> FFmpeg -> 网络输出
```

- 缩放路径：`1080P -> 720P`
- 作用：
  - 远程预览
  - 低带宽场景下的辅助输出

### 3. 会议 AI 分析流

```text
VI
-> OpenCV 缩放到 640x360 分析帧
-> RockX 人脸检测
-> 将检测框映射回原始分辨率
-> 主目标稳定跟踪 + ROI 轻量美颜
-> VENC(2)
-> ./video_conference_ai_venc.h264
```

- 作用：
  - 直播 / 视频会议预览增强
  - AI 结果视频输出
  - 为后续主讲人跟踪、会议画面增强、多脸切换等场景做扩展准备

## 设计思路

### 1. 双码流不是重复功能，而是链路分层

- 主码流负责高质量输出
- 预览码流负责弱网预览和轻量分发
- AI 分析流负责智能分析和增强结果输出

这样拆分之后，更符合资源受限端侧平台的工程设计习惯。

### 2. AI 不直接绑定编码高码流

当前 AI 支路不是直接消费编码后的 `H.264` 主码流，而是从 `VI` 原始图像一侧旁路取帧，独立构建分析链路。

这样做的好处是：

- 避免“编码流 -> 解码 -> 再推理”的额外损耗
- 避免 AI 与推流链路强耦合
- 更利于控制时延和系统稳定性

### 3. 低分辨率检测 + 原图 ROI 增强

对于直播 / 视频会议场景，持续对整张高分辨率图像做人脸检测通常不是最优方案。

当前代码采用的是：

- 低分辨率分析帧做人脸检测
- 原始分辨率图像做人脸 ROI 跟踪和轻量美颜

这种方式更符合端侧实时系统对：

- 算力
- 时延
- 视觉效果
- 稳定性

之间的平衡。

## 核心代码结构

### 主要源码文件

- `rv1126_ffmpeg_main.cpp`
  - 程序入口
- `rkmedia_module_function.cpp`
  - 初始化 `VI / VENC / RGA`
- `rkmedia_assignment_manage.cpp`
  - 进行模块绑定并启动线程
- `rkmedia_data_process.cpp`
  - 从 `VENC` 取码流，写入队列，并交给 FFmpeg
- `rkmedia_ffmpeg_config.cpp`
  - 创建 FFmpeg 输出上下文
- `rockx_video_conference_module.cpp`
  - 会议 AI 分析支路：
    - 低分辨率人脸检测
    - 主脸稳定跟踪
    - ROI 轻量美颜
    - VENC(2) 输出
- `rv1126_isp_function.cpp`
- `sample_common_isp.c`
  - ISP / RKAIQ 相关封装

### 相关文档

- `project_code_summary.md`
- `project_resume.md`
- `camera_expansion_roadmap.md`
- `camera_isp_control_layer_design.md`
- `video_conference_beauty_tracking_summary.md`

## 依赖环境

本项目面向 `RV1126 Linux SDK` 风格的交叉编译环境。

主要依赖包括：

- RKMedia
- Rockchip MPP
- RGA
- RKAIQ
- FFmpeg
- OpenCV
- RockX
- SDL / SDL_ttf / FreeType

仓库内也保留了当前工程所依赖的部分板端库和第三方预编译资源。

## 编译说明

仓库当前保留了一份面向 RV1126 板端环境的 `Makefile`。

典型编译命令：

```bash
make
```

当前 `Makefile` 使用的是交叉编译器路径，例如：

```makefile
/opt/rv1126_rv1109_linux_sdk_v1.8.0_20210224/.../arm-linux-gnueabihf-g++
```

在不同开发环境里，通常需要根据自己的 SDK 实际路径调整：

- 工具链路径
- sysroot / SDK 路径
- OpenCV 头文件和库路径
- RockX 头文件和库路径

## 运行方式

当前程序需要 4 个参数：

```bash
./rv1126_ffmpeg_main high_stream_type high_url_address low_stream_type low_url_address
```

示例：

```bash
./rv1126_ffmpeg_main 0 rtmp://192.168.1.10/live/high 0 rtmp://192.168.1.10/live/low
```

代码里定义的协议类型：

- `0`：`FLV_PROTOCOL`
- `1`：`TS_PROTOCOL`

## 当前输出

### 网络输出

- 主码流：
  - `1080P H.264 -> FFmpeg -> 网络`
- 预览码流：
  - `720P H.264 -> FFmpeg -> 网络`

### 本地输出

- 会议 AI 结果流默认保存到：

```bash
./video_conference_ai_venc.h264
```

## 当前项目状态

### 已完成

- `VI -> VENC(0)` 主码流链路
- `VI -> RGA -> VENC(1)` 预览码流链路
- `VENC -> 队列 -> FFmpeg` 异步解耦结构
- `RockX + OpenCV` 会议 AI 分析支路
- AI 增强结果的独立编码输出
- ISP 相关初始化封装

### 仍可继续扩展

- `AI / AENC` 音频采集与编码
- 音视频同步与复用
- 更完整的 ISP 控制接入
- Camera 控制层抽象
- 更贴近会议系统的 AI 调度和多脸处理

## 后续值得推进的方向

- 从框级跟踪升级到关键点级人脸对齐 / 跟踪
- 加入多脸排序和主讲人切换逻辑
- 增加 AI 帧采样和负载控制
- 将 ISP 参数控制真正接到主流程里
- 将会议 AI 支路从本地 H.264 输出继续扩展到预览流或 WebRTC 风格输出

## 说明

- 本仓库依赖 RV1126 板端 SDK 环境
- `Makefile` 中的路径在其他环境下大概率需要重新对齐
- 当前会议 AI 支路更偏工程方向验证，不是量产级美颜方案
- 本次会议 AI 代码改动没有在当前本地环境做实际编译和运行验证
