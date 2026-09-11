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
      主线程（只做轻活）                       worker 线程（重活：编码+推流全在这）
┌──────────────────────────────┐        ┌──────────────────────────────────────┐
│ upVideo：拷 y/uv 入队 + 预览   │──入队──→│ vqueue(丢最老) → 三档独立 sws+H.264+写FLV │
│ onAuReadAll：麦克风 append      │──串流──→│ fifo(攒块) → AAC 编一次 → clone×3 写    │
└──────────────────────────────┘        └──────────────────────────────────────┘
```

- **采集 + 音频编码** 共享（音频三路内容一样，编一次克隆）
- **视频编码 + 输出连接** 每路独立（分辨率不同分开编，地址不同分开推）
- **线程拆分工**：主线程只做「拷数据入队 + 画预览」，编码/推流/重连全在 worker 线程（生产者-消费者）
- **视频队列**和**音频 fifo** 是两套独立通道：视频丢最老保最新，音频攒块不丢（见设计亮点 5）

## 目录结构

| 文件 | 职责 |
|------|------|
| widget.h/cpp | 主线程：采集帧拷数据入队 + 麦克风采集转发 + 预览渲染 |
| camera.h/cpp | 摄像头采集封装（QCamera + QMediaCaptureSession + QVideoSink） |
| audioformat.h/cpp | 麦克风采集封装（QAudioSource） |
| av.h/cpp | worker 线程：三档编码、封装、推流、抽帧、重连、音频编码 |
| framequeue.h/cpp | 线程安全视频帧队列（QMutex + QWaitCondition，丢最老保最新） |
| streamlane.h/cpp | 一路（一档清晰度）推流的封装：编码器 / 输出连接 / 重连状态 |
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
   摄像头回调「约 30fps」≠ 目标帧率（360p 要 25fps），必须自己抽帧。每路维护虚拟时钟 `nextpts`：编一帧 `nextpts += 1000.0/fps`，当前帧 `now >= nextpts` 才编码。不能用「距上次编码时刻」判断——30fps 采集配 40ms 阈值会退化成 66.67ms 一帧（15fps），目标帧率永远到不了。FLV 时间基固定 `{1,1000}`，编码器时间戳必须先 `av_packet_rescale_ts` 换算，否则视频 1000fps。

5. **线程模型：主线程轻、worker 重——生产者-消费者分离**
   编码是重活，全放主线程时，一帧编码可能超过采集回调间隔（30fps 约 33ms），主线程事件循环被占死 → 后续信号/重绘全排队 → UI 卡 + 掉帧。拆成「主线程只做采集拷数据 + 画预览」「worker 线程做编码/推流/重连」，用队列交接。
   - **视频为什么丢最老保最新**：`vqueue` 上限 5，满了 `pop_front` 丢最老。理由①实时——观众只看最新；②防积压——不然延迟随消费跟不上无限涨；③防内存爆——一帧 NV12 约 1.3MB，深 5 可控。**不能改成「满了阻塞生产端」**——生产端在主线程，一阻塞 UI 就死。
   - **音频为什么攒块不丢**：人耳对中断极敏感，丢帧破音，跟视频丢帧人眼几乎无感不同。所以音频走独立 `fifo`，攒够 4096B（1024 样本 × 2ch × 2B）才编、内部 `while` 全清光；锁只罩取数那一下，编码/写流在锁外，网络卡多久都不挡主线程。
   - **为什么不直接传 QVideoFrame**：它是引用计数句柄、**不复制数据**，底层内存属于相机、下一帧可能被覆写——必须先拷 y/uv 进 `Nv12Frame` 再入队（见 framequeue.h 注释）。
   - **关闭顺序**：`stopFrameQueue`（`wakeAll` 唤醒阻塞的 `pop` → 返回空帧 → run 里 `frame.width==0` break）→ `requestInterruption`（双保险）→ `wait`（等线程真结束，防悬垂）。只中断不叫醒队列，线程会睡死在条件变量上。

## 踩坑记录

- `max_b_frames` 设 0，直播禁 B 帧（负 dts）
- AAC priming：FFmpeg 8.0.1 native AAC 首包 pts=2048（encoder delay 2048 采样），无负 pts，`aPts` 从 0 即可。仅旧版本编码器会吐负包，才需 `aPts` 初始 +1024 补偿
- `av_packet_clone` 克隆音频包（`av_packet_init` 不存在，`av_packet_ref` 要预 init）
- 写头必须等第一帧 `receive_packet` 之后（codecpar 的 SPS/PPS 那时才就绪）
- QAudioSource `readyRead` 必须先 `readAll` 读空，否则缓冲满信号不再触发
- **重连时别重置视频时钟（09-11）**：`elap` 一人两职——抽帧参照 **和** 视频 pts（`frame->pts=now`）。`elap.restart()` 会把**视频 pts 归零**；而音频 `aPts` **不能**归零（`aucodec` 全局三路共享、从不重建，归零会波及未重连的两路 + 音频时间戳回退）→ 音视频**零点错开**，拉流端黑屏/有声无画。**正解：删掉 `elap.restart()`（视频 pts 保持连续），保留 `nextpts=0`（重锚哨兵：下一帧 `if(nextpts==0) nextpts=now`，且保证重连首帧必编）。音频 `aPts` 一个字别动。**

## 待优化

- 内存泄漏排查（Dr.Memory / valgrind）
- `setupOutput` 依赖 `setupAudio` 成功，音频打开失败时需跳过音频流
