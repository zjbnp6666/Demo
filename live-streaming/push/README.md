# QCamera —— 摄像头音视频多码率推流端

基于 Qt Multimedia 采集摄像头 + 麦克风，经 FFmpeg 编码后，以三档清晰度（多码率）同时推送到 RTMP 服务器，实现直播平台的「清晰度切换」能力。

## 功能特性

- 摄像头 + 麦克风实时采集（Qt Multimedia）
- H.264 三档清晰度编码：高清 1280x720 / 标清 640x360 / 流畅 320x180
- AAC 音频编码一次、克隆到三路（共用一个编码器）
- 三路 RTMP 同时推流
- 断线自动重连 + 退避（写流失败不卡白屏）
- **音频设备热插拔自适应**（拔耳机自动切到当前输入设备，不用重启）
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
   摄像头回调「约 30fps」≠ 目标帧率（360p 要 25fps），必须自己抽帧。每路维护虚拟时钟 `nextpts`：编一帧 `nextpts += 1000.0/fps`，当前帧 `now >= nextpts` 才编码。不能用「距上次编码时刻」判断——30fps 采集配 40ms 阈值会退化成 66.67ms 一帧（15fps），目标帧率永远到不了。FLV 时间基固定 `{1,1000}`，编码器时间戳必须先 `av_packet_rescale_ts` 换算，否则视频 1000fps。

5. **码率控制：直播要 CBR，"设个码率"远不够**
   直播带宽恒定 → 三路各锁 **CBR**。关键是三笔：① **`rc_max_rate` / `rc_min_rate` 都设成目标码率——`min=max` 才是 CBR**；只给 `bit_rate` 是 **ABR**（锁平均、瞬时能飙、会顶爆上行带宽）；② **`rc_buffer_size = 码率 × 2` 当"桶"**——桶 = 编码器**攒 bit 的空间**，太小则复杂场景被猛压（实测 PSNR 掉 9.5 dB），太大则延迟高，**2× 是直播经验点**；③ 配合已有的 `max_b_frames=0` + `rc-lookahead=0` 一起压首包延迟。
   > 软编（libx264）**没有"模式开关"**，模式由**参数组合**推出来；硬编（NVENC 等）才有显式 `-rc cbr`。

## 踩坑记录

- `max_b_frames` 设 0，直播禁 B 帧（负 dts）
- AAC priming：FFmpeg 8.0.1 native AAC 首包 pts=2048（encoder delay 2048 采样），无负 pts，`aPts` 从 0 即可。仅旧版本编码器会吐负包，才需 `aPts` 初始 +1024 补偿
- `av_packet_clone` 克隆音频包（`av_packet_init` 不存在，`av_packet_ref` 要预 init）
- 写头必须等第一帧 `receive_packet` 之后（codecpar 的 SPS/PPS 那时才就绪）
- QAudioSource `readyRead` 必须先 `readAll` 读空，否则缓冲满信号不再触发
- **重连时别重置视频时钟（09-11）**：`elap` 一人两职——抽帧参照 **和** 视频 pts（`frame->pts=now`）。`elap.restart()` 会把**视频 pts 归零**；而音频 `aPts` **不能**归零（`aucodec` 全局三路共享、从不重建，归零会波及未重连的两路 + 音频时间戳回退）→ 音视频**零点错开**，拉流端黑屏/有声无画。**正解：删掉 `elap.restart()`（视频 pts 保持连续），保留 `nextpts=0`（重锚哨兵：下一帧 `if(nextpts==0) nextpts=now`，且保证重连首帧必编）。音频 `aPts` 一个字别动。** **下游症状（同一病根）：** 错乱的时间戳会让订阅端（ffmpeg/客户端）**每 30 秒断一次**（`End of file`→重连）；PTS 修好后这个 30 秒断随之消失——一个根，两个果。
- **别凭"感觉"判断摄像头帧率（09-14）**：代码注释里写着"摄像头看似 30fps 其实只有 10"，并据此把 GOP 设成 10。
  拉流端实测把它推翻了——**视频包 30/s + 音频包 43/s = 73 ≈ 收包 71/s**，**就是 30fps**。
  帧率 / GOP / 码率这类"参数假设"，**要实测（数包）**，别信注释和感觉。

- **拔耳机后麦克风「死掉不重开」（09-16 修）**：`QAudioSource` 原来**只在构造时取一次**
  `defaultAudioInput()`。设备一换（拔耳机），旧设备没了，source 收不到数据、**也不会自己重开**
  → 流里音频变成**真空**（`volumedetect` 实测 **-91dB**），并且**不会恢复**。
  **正解两条**：① `AudioFormat::start()` 里**每次都重新取** `defaultAudioInput()`（别只在构造时取）；
  ② 监听 `QMediaDevices::audioInputsChanged` → `relatchMic()` 重建 source。
  **⚠️ 坑中坑**：重建 `QAudioSource` 之后 **`readyRead` 必须重连到新的 `io`** —— 漏了这一步，
  数据永远不再来，看起来"修了但没用"。
- **音频断 ≠ 只影响音频**：音频轨一直没数据时，`av_interleaved_write_frame` 会**持续攒包**等下一条轨
  （攒到 `max_interleave_delta`，默认 **10 秒**才强制吐出）→ **整条流的延迟涨到 10~13 秒，而且不会自己降**。
  **⇒ 延迟是"音频死"的下游症状，修好麦克风重开它自己就没了。**

## 待优化

- 内存泄漏排查（Dr.Memory / valgrind）
- `setupOutput` 依赖 `setupAudio` 成功，音频打开失败时需跳过音频流
