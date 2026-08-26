# QCamera —— 摄像头音视频多码率推流端

基于 Qt Multimedia 采集摄像头 + 麦克风，经 FFmpeg 编码后，以三档清晰度（多码率）同时推送到 RTMP 服务器，实现直播平台的「清晰度切换」能力。

## 功能特性

- 摄像头 + 麦克风实时采集（Qt Multimedia）
- H.264 三档清晰度编码：高清 1280x720 / 标清 640x360 / 流畅 320x180
- AAC 音频编码一次、克隆到三路（共用一个编码器）
- 三路 RTMP 同时推流
- 断线自动重连 + 退避（写流失败不卡白屏）
- 分级日志（Logger 单例，写 app.log）

## 技术栈

- Qt 6（Widgets / Multimedia）
- FFmpeg（avcodec / avformat / swscale / swresample）
- C++17（智能指针 / RAII）

## 架构

```
摄像头 ─┐                              ┌─→ 720p H.264 ─→ FLV ─→ rtmp://.../stream_720p
        ├─→ 采集 ─┬─ 视频：三档独立编码 ─┼─→ 360p H.264 ─→ FLV ─→ rtmp://.../stream_360p
麦克风 ─┘         │                     └─→ 180p H.264 ─→ FLV ─→ rtmp://.../stream_180p
                  └─ 音频：AAC 编一次 ──→ 克隆三路，分别写入上面三个连接
```

- **采集 + 音频编码** 共享（音频三路内容一样，编一次克隆）
- **视频编码 + 输出连接** 每路独立（分辨率不同分开编，地址不同分开推）

## 目录结构

| 文件 | 职责 |
|------|------|
| widget.h/cpp | 主逻辑：三路编码、封装、推流、重连、抽帧 |
| camera.h/cpp | 摄像头采集封装（QCamera + QMediaCaptureSession + QVideoSink） |
| audioformat.h/cpp | 麦克风采集封装（QAudioSource） |
| logger.h/cpp | 分级日志单例，写 app.log |
| main.cpp | 程序入口 |

## 构建与运行

依赖：MSYS2 的 FFmpeg + Qt6 Multimedia。**必须用 MSYS2 kit 编译**（GCC 版本不匹配启动即崩）。

```bash
# 1. 启动 RTMP 服务器
cd D:/QTT/nms-server && node app.js

# 2. Qt Creator 用 MSYS2 kit 编译运行 QCamera

# 3. 拉流验证（任选一路）
ffplay rtmp://127.0.0.1:1935/live/stream_720p
ffplay rtmp://127.0.0.1:1935/live/stream_360p
ffplay rtmp://127.0.0.1:1935/live/stream_180p
```

## 设计亮点

1. **「共享 vs 独立」的资源划分——多码率的灵魂**
   采集只有一路（一个摄像头/麦克风），编码却分三档。识别出：音频三路内容一样 → 只编一次、`av_packet_clone` 克隆三路（引用计数，不拷数据）；视频分辨率不同 → 每路独立编码器/缩放器。sws 输入用采集分辨率、输出用档位分辨率，一次采集喂三档。

2. **RAII 包装 C API——C 资源有了 C++ 的生命周期**
   FFmpeg 全是裸指针 + 手动 free，少 free 一个就漏。用 `unique_ptr` + 自定义删除器包装 AVCodecContext/AVFrame/AVFormatContext，函数任何地方 return 都自动释放，十几处错误分支零泄漏，不用写 `goto cleanup`。

3. **退避重连——「自我修复」但不「自残」**
   `avio_open` 连不上阻塞约 2 秒，断流后每帧重试会卡成白屏。`retryArmed` + 3 秒退避，失败后冷却再试；重连只重置这一路（`resetStream`），不碰共享音频、不连累另两路。

4. **时间是推流最隐蔽的坑**
   摄像头帧率是「约 30」不是「精确 30」，数帧计数会越漂越远，用 `QElapsedTimer` 真实时钟 + `lastEncodeMs` 抽帧。FLV 时间基固定 `{1,1000}`，编码器时间戳必须先 `av_packet_rescale_ts` 换算，否则视频 1000fps。

## 踩坑记录

- `max_b_frames` 设 0，直播禁 B 帧（负 dts）
- AAC priming，首包 pts=-1024，`aPts` 初始 +1024 补偿
- `av_packet_clone` 克隆音频包（`av_packet_init` 不存在，`av_packet_ref` 要预 init）
- 写头必须等第一帧 `receive_packet` 之后（codecpar 的 SPS/PPS 那时才就绪）
- QAudioSource `readyRead` 必须先 `readAll` 读空，否则缓冲满信号不再触发

## 待优化

- 内存泄漏排查（Dr.Memory / valgrind）
- `setupOutput` 依赖 `setupAudio` 成功，音频打开失败时需跳过音频流
