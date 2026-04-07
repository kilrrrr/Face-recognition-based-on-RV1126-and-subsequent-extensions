# 基于 RV1126 的嵌入式音视频前端、ISP 控制与会议 AI 分析 Demo

## 项目简介

本项目基于 Rockchip RV1126 平台实现一套板端嵌入式音视频前端，当前已经覆盖三部分能力：

- 视频采集、缩放、硬件编码与 FFmpeg 推流
- 会议场景下的 `RockX + OpenCV` 人脸检测、稳定跟踪与轻量美颜
- `ISP / RKAIQ` 控制层接入，用于成像质量控制和运行时场景调节

因此，这个项目已经不只是一个简单的推流 Demo，而是更接近：

`嵌入式音视频前端 + Camera / ISP 控制层 + 会议 AI 分析支路`

## 当前结构

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
  - 低带宽辅助输出

### 3. 会议 AI 分析流

```text
VI
-> OpenCV 缩放到 640x360 分析帧
-> RockX 人脸检测
-> 检测框映射回原始分辨率
-> 主目标稳定跟踪 + ROI 轻量美颜
-> VENC(2)
-> ./video_conference_ai_venc.h264
```

### 4. ISP 控制层

```text
main()
-> 初始化 ISP / RKAIQ
-> 下发默认 ISP profile
-> 启动 VI / RGA / VENC / 推流 / AI 线程
-> 启动 isp_control_thread
-> 周期查询 AE / AWB / CCT 信息
-> 在 day / night / backlight profile 间切换
-> 动态调节曝光、白平衡、WDR、日夜模式、LDCH、亮度、对比度、饱和度
```

这意味着当前项目已经具备了“数据面 + 控制面”分层：

- 数据面负责采、编、传和 AI 视频处理
- 控制面负责成像质量控制

## 设计思路

### 1. 双码流不是重复功能，而是职责分层

- 主码流负责高质量输出
- 预览码流负责低带宽预览
- AI 分析流负责智能分析和增强结果输出

### 2. AI 不直接绑定编码高码流

会议 AI 支路不是直接消费编码后的高码流，而是从 `VI` 一侧旁路取帧独立分析，这样更利于：

- 控制时延
- 降低耦合
- 便于后续扩展 AI 调度

### 3. ISP 从“底层接口”升级为“控制层”

仓库原本就有 `SAMPLE_COMM_ISP_*` 底层接口，但这次已经把它们真正接入了主流程，形成：

- 启动期默认 profile 下发
- 运行时元数据查询
- 场景判断
- profile 切换
- 动态参数控制

这让项目从“能传视频”进一步变成“能控制成像质量”的 camera 前端系统。

## 主要源码文件

- `rv1126_ffmpeg_main.cpp`
  - 程序入口，先初始化 ISP 控制层，再启动视频主链路
- `rkmedia_module_function.cpp`
  - 初始化 `VI / VENC / RGA`
- `rkmedia_assignment_manage.cpp`
  - 模块绑定、线程启动，并启动 `isp_control_thread`
- `rkmedia_data_process.cpp`
  - 从 `VENC` 取码流，写入队列，并交给 FFmpeg
- `rkmedia_ffmpeg_config.cpp`
  - 创建 FFmpeg 输出上下文
- `rockx_video_conference_module.cpp`
  - 会议 AI 分析支路
- `rv1126_isp_function.cpp`
  - ISP 初始化封装
- `rv1126_isp_control_layer.cpp`
  - ISP 控制层实现
- `sample_common_isp.c`
  - RKAIQ 底层控制和查询封装

## 相关文档

- `project_code_summary.md`
- `camera_expansion_roadmap.md`
- `camera_isp_control_layer_design.md`
- `video_conference_beauty_tracking_summary.md`
- `isp_control_layer_integration_summary.md`

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

## 编译

仓库当前保留了一份面向 RV1126 板端环境的 `Makefile`。

典型编译命令：

```bash
make
```

当前编译目标已包含：

- 视频推流链路
- 会议 AI 分析链路
- ISP 控制层

## 运行方式

当前程序需要 4 个参数：

```bash
./rv1126_ffmpeg_main high_stream_type high_url_address low_stream_type low_url_address
```

示例：

```bash
./rv1126_ffmpeg_main 0 rtmp://192.168.1.10/live/high 0 rtmp://192.168.1.10/live/low
```

协议类型：

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
- `ISP / RKAIQ` 初始化、profile 下发和运行时控制线程接入
- 基于亮度 / lux / luma deviation 的 day / night / backlight 场景切换

### 仍可继续扩展

- `AI / AENC` 音频采集与编码
- 音视频同步与复用
- 更完整的多场景 ISP tuning profile
- ISP metadata 与 AI / 录像 / 抓拍联动
- 更贴近会议系统的多脸切换和 AI 调度

## 说明

- 本仓库依赖 RV1126 板端 SDK 环境
- `Makefile` 中的路径在其他环境下通常需要重新对齐
- 当前 ISP 控制层属于贴合现有项目结构的工程化版本，还不是完整量产级调优系统
- 本次新增代码没有在当前本地环境做实际编译和运行验证
