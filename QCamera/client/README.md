# QCamera-Client —— RTMP 拉流直播播放器

从 RTMP 服务器拉流，解封装、解码、音视频同步渲染的直播播放器，支持三档清晰度切换。

## 功能特性

- RTMP 拉流 → 解封装 → 解码 → 渲染
- 音视频同步（音频主时钟，落后丢帧 / 超前缓存）
- 清晰度切换（高清 / 标清 / 流畅三档）
- 断线自动重连（rw_timeout 读超时）
- 音量调节 + 静音
- 画面缩放三策略（等比 / 拉伸 / 裁剪）
- 暂停 / 全屏

## 技术栈

- Qt 6（Widgets / Multimedia）
- FFmpeg（avformat / avcodec / swscale / swresample）
- C++17（智能指针 / 多线程 / 策略模式）

## 架构

```
RTMP 流 ─→ 解封装 ─┬─→ 视频解码 ─→ sws(YUV→RGB) ─→ 帧队列 ─→ 30ms 定时器 ─→ 渲染
                   └─→ 音频解码 ─→ swr(FLTP→S16) ─→ 音频队列 ─→ QAudioSink 播放
                                              ▲
                         音频主时钟（QAudioSink::processedUSecs）
```

## 目录结构

| 文件 | 职责 |
|------|------|
| avdeio.h/cpp | 拉流解码线程（解封装 + 音视频解码） |
| tqueue.h/cpp | 线程安全队列（视频/音频，QMutex + QWaitCondition） |
| audiosinke.h/cpp | 音频播放封装（QAudioSink） |
| videowidget.h/cpp | 画面渲染 |
| renderstrategy.h/cpp | 画面缩放策略（等比/拉伸/裁剪） |
| widget.h/cpp | UI + 音视频同步 + 控制 |
| logger.h/cpp | 分级日志 |

## 构建与运行

依赖：MSYS2 的 FFmpeg + Qt6 Multimedia，MSYS2 kit 编译。需先有推流端 QCamera 或任一路 RTMP 流。

```bash
# 1. 起 RTMP 服务器 + 推流端 QCamera
# 2. 编译运行 QCamera-Client，默认拉 720p
# 3. 点左上角「高清 / 标清 / 流畅」切换清晰度
```

## 设计亮点

1. **音频主时钟用「播放位置」，不是「帧到达时间」**
   最容易犯的错：拿最新解码帧的 pts 当时钟。但帧「到达」≠「已播放」，用到达时间当时钟会把时钟拨快，视频永远晚到被丢。用 `QAudioSink::processedUSecs`（声卡真实播到哪）当时钟，视频 pts 跟播放位置比才对得上。

2. **换流要「重锁时钟基准」**
   切清晰度/断线重连后，视频 pts 和播放位置都从头开始，旧基准会让视频 pts 超前、每帧都走缓存、帧率减半像慢放。换流时 `clockBasePts`/`procBase` 置 0 重新锁定。

3. **策略模式做画面缩放——开闭原则**
   等比/拉伸/裁剪三种缩放抽象成 `Renderstrategy` 基类 + 三个子类，VideoWidget 只认基类。加新缩放方式不改旧代码。

4. **断线重连的「最后一公里」**
   `rw_timeout` 设 3 秒（否则断流后 `av_read_frame` 卡 socket 几十秒）；重连用 `avformat_close_input` 关 socket（`free_context` 只清内存，连接还挂着）；断流后消费者阻塞 pop 卡死主线程，pop 前先 isEmpty 判空。

## 踩坑记录

- 阻塞 pop 死锁——断流后消费者阻塞 pop 卡死主线程，pop 前先 isEmpty 判空
- QImage 裸指针构造必须 copy() 再 free（引用不拥有内存）
- sws_getContext 要等首帧成功后建（那时才有宽高）
- 音频包 unref + return，别送视频解码器

## 待优化

- 内存泄漏排查（Dr.Memory / valgrind）
