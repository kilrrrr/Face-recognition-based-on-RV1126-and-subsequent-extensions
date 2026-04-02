# RockX + OpenCV 人脸检测模块接入说明

## 1. 这次新增的目标

这次是在原有“高低码流推流”基础上，新增了一条独立的人脸检测处理支路，整体思路按你给的流程图来实现：

1. 保留原来的高码流推流和低码流推流
2. 额外从 `VI` 取原始图像帧
3. 用 `RockX` 做人脸检测
4. 用 `OpenCV` 在图像上画框和打标签
5. 把处理后的图像重新送入一个新的 `VENC` 编码通道
6. 单独起线程从这个新的 `VENC` 中取出 H264 码流并保存为文件

也就是说，现在工程里一共有三条视频相关支路：

- 原高码流推流支路
- 原低码流推流支路
- 新增的人脸检测编码支路

## 2. 新增/修改的文件

### 新增文件

- `rockx_face_detect_module.h`
- `rockx_face_detect_module.cpp`
- `rockx_opencv_face_detect_summary.md`

### 修改文件

- `rkmedia_module_function.cpp`
- `rkmedia_assignment_manage.cpp`
- `Makefile`

## 3. 代码接入后的新流程

### 原流程

原来项目主流程是：

- `VI -> VENC(0) -> high_video_queue -> FFmpeg -> 高码流推流`
- `VI -> RGA -> VENC(1) -> low_video_queue -> FFmpeg -> 低码流推流`

### 新增的人脸检测流程

现在新增了一条：

- `VI -> rockx_face_detect_thread -> RockX 人脸检测 -> OpenCV 画框 -> VENC(2) -> rockx_face_detect_venc_thread -> 保存 H264 文件`

## 4. 每个新增点在干什么

### 4.1 `rockx_face_detect_module.h`

这个头文件定义了人脸检测模块的公共接口和参数结构。

主要内容：

- `ROCKX_FACE_DETECT_VENC_ID`
  - 检测结果编码通道固定使用 `VENC(2)`

- `ROCKX_FACE_DETECT_OUTPUT_PATH`
  - 检测结果的编码文件默认输出到：
    `./rockx_face_detect_venc.h264`

- `ROCKX_FACE_DETECT_PARAM`
  - 保存检测线程和编码保存线程所需参数
  - 包含：
    - `vi_id`
    - `venc_id`
    - `width`
    - `height`
    - `output_path`

### 4.2 `init_rockx_face_detect_venc_module()`

这个函数在 `rockx_face_detect_module.cpp` 中实现。

作用：

- 新建一个专门给人脸检测结果使用的 `VENC(2)`
- 编码格式是 `H.264`
- 分辨率是 `1920x1080`
- 编码参数和主码流基本保持一致
- 初始化成功后，把 `VENC(2)` 的通道号写入全局容器

### 4.3 `rockx_face_detect_thread()`

这是新增的人脸检测主线程。

它负责：

1. 从 `VI` 直接取原始图像帧
2. 把带 stride 的 `NV12` 图像整理成紧凑连续内存
3. 调用 RockX 检测人脸
4. 调用 OpenCV 在图像上画框
5. 再把处理后的图像转回 `NV12`
6. 创建新的 `MEDIA_BUFFER`
7. 把这个处理后的 `MEDIA_BUFFER` 发送给 `VENC(2)`

这个线程是这次新增功能的核心。

### 4.4 `rockx_face_detect_venc_thread()`

这是新增的编码码流保存线程。

它负责：

1. 从 `VENC(2)` 取 H264 编码后的码流
2. 把码流持续写入 `./rockx_face_detect_venc.h264`

它对应你流程图中的：

- “从 VENC 模块获取到 H264 编码后的视频”
- “保存 H264 的 VENC 码流视频数据”

## 5. 这次对现有工程的接线点

### 5.1 `rkmedia_module_function.cpp`

在原本初始化完高码流 `VENC(0)`、低码流 `VENC(1)` 后，新增调用：

```cpp
init_rockx_face_detect_venc_module();
```

作用就是把检测支路专用的 `VENC(2)` 也创建出来。

### 5.2 `rkmedia_assignment_manage.cpp`

在原有高低码流线程启动完成后，新增启动两个线程：

- `rockx_face_detect_thread`
- `rockx_face_detect_venc_thread`

这样做的结果是：

- 原来的推流逻辑继续跑
- 新的人脸检测编码逻辑并行跑

### 5.3 `Makefile`

这次我把以下内容接进了 `Makefile`：

- 新增编译源文件：`rockx_face_detect_module.cpp`
- 新增 OpenCV 头文件路径：`./opt/arm_opencv/include`
- 新增 OpenCV 库路径：`./opt/arm_opencv/lib`
- 新增链接库：
  - `-lopencv_core`
  - `-lopencv_imgproc`
  - `-lopencv_imgcodecs`
  - `-lrockx`

## 6. RockX / OpenCV 适配方式

### RockX

新模块没有把 RockX 头文件路径写死，而是做了头文件存在性判断：

- `rockx.h`
- `<rockx.h>`
- `<rockx/rockx.h>`

如果找不到 RockX SDK 头文件：

- 线程仍然会运行
- 但只会把原始帧透传给 `VENC(2)`
- 不会做真正的人脸检测

这样做的目的是让代码在没有完整 SDK 头文件时也尽量不阻塞整体接入。

### OpenCV

OpenCV 也是类似处理：

- 如果 `opencv2/opencv.hpp` 可用，就执行颜色转换、画框、打标签
- 如果不可用，就不画框，只转发原始图像

## 7. 新模块内部实现细节

### 7.1 图像格式处理

当前检测线程输入和输出都按 `NV12` 处理。

为了兼容 RKMedia 图像缓冲中可能存在的 stride，代码里新增了两个辅助函数：

- `copy_strided_nv12_to_packed()`
  - 把带 stride 的 NV12 图像整理成紧凑布局

- `copy_packed_nv12_to_strided()`
  - 把紧凑布局的 NV12 再写回带 stride 的缓冲

### 7.2 OpenCV 画框

当检测到人脸后，会在图像上画：

- 绿色框
- 置信度文本
- 左上角的人脸数量统计

### 7.3 BGR 转回 NV12

OpenCV 画框过程使用的是 BGR 图像，所以画完后又做了一次：

- `BGR -> I420 -> NV12`

最终重新送回 RKMedia 编码器。

## 8. 你现在可以重点看的代码位置

如果你要继续读这部分，推荐顺序：

1. `rockx_face_detect_module.h`
2. `rockx_face_detect_module.cpp`
3. `rkmedia_module_function.cpp`
4. `rkmedia_assignment_manage.cpp`

其中最关键的函数是：

- `init_rockx_face_detect_venc_module()`
- `rockx_face_detect_thread()`
- `rockx_face_detect_venc_thread()`

## 9. 当前实现的定位

这次生成的代码定位是：

- 已经把“接入点、线程模型、图像处理链路、编码保存链路”全部补齐
- 已经把 RockX 和 OpenCV 的代码骨架接进现有工程
- 保持原推流逻辑不变

更直白一点说，现在这份代码已经把“人脸检测支路”的工程结构搭好了。

## 10. 一句话总结

这次新增的是一条独立于原推流链路的人脸检测编码支路：从 `VI` 取原始帧，经 `RockX` 做检测、`OpenCV` 做画框，再送入新的 `VENC(2)` 编码，并由单独线程把 H264 结果保存下来；这样既保留了现有推流能力，又把你流程图里的 RockX + OpenCV 人脸检测处理链路接进了项目。
