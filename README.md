# QT FFmpeg 视频播放器

基于 Qt6 + FFmpeg 的多线程视频播放器，支持音视频同步、倍速播放、进度条 seek 等功能。

## 功能

| 功能 | 操作 |
|---|---|
| 打开视频 | 启动时选择，或拖拽文件到窗口 |
| 暂停/播放 | 空格键 |
| 进度条 Seek | 拖动底部进度条 |
| 音量调节 | 右侧垂直滑条 |
| 倍速 1.0x / 1.5x / 2.0x | 数字键 1 / 2 / 3 |
| 按住快进 (1.5x) | 按住左箭头，松开恢复 |
| 全屏 | 双击画面，Esc 退出 |
| 截图 | S 键，保存到桌面 |
| 播放列表 | 点击「≡ 列表」按钮，支持拖拽排序 |

## 架构

```
主线程 (Widget)                  解码线程 (DecoderWorker)
     │                                  │
     │  QTimer 每 16ms                   │  QTimer 每 16ms
     │  onPlaybackTick()                │  decodeBatch()
     │       ↓                          │       ↓
     │  pop() ←──── FrameQueue ──── push()
     │       ↓     (容量 5, 互斥+条件变量)   │
     │  音视频同步                         │  FFmpeg 解封装
     │  paintEvent()                     │  YUV→RGB / 音频重采样
     │                                   │
     └────── requestSeek ──────────→ seek()
                     isSeekFalse ←─────────┘
```

## 技术栈

- **UI:** Qt 6.7 Widgets + QSS 美化
- **解码:** FFmpeg 6.x（avformat / avcodec / swscale / swresample）
- **音频:** QAudioSink
- **线程:** QThread + moveToThread，跨线程信号通信
- **同步:** 条件变量 + 互斥锁实现的阻塞队列

## 构建

```bash
# 需要 Qt 6.7+ 和 FFmpeg 开发库
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

## 踩坑记录

### Bug 1: seek 后死锁 — decodeOneVideoFrame 一直读到 EOF

**现象：** 拖动进度条后画面卡死，debug 发现 `isSeekFalse` 信号从未发出。

**原因：** `decodeOneVideoFrame()` 函数名说"只解码一帧"，但内层 `while(avcodec_receive_frame)` 的 `break` 只退出内层循环，外层 `while(av_read_frame)` 继续读到文件末尾。帧队列满（容量 5）→ `push()` 阻塞等待消费者取帧 → 但消费者 `strat()` 被 `m_isSeeking = true` 拦住 → `isSeekFalse` 永远发不出 → 死锁。

**教训：** `break` 只退一层，多层循环里想退到底用 `return`。以及函数名不要骗人。

**修复：** 内层 `while` → `if`，解码成功一帧后 `return`。

### Bug 2: seek 信号被 QTimer 事件挤掉

**现象：** 直接从开头拖到远处卡死，但如果视频已经播放过那个位置再拖回去就正常。

**原因：** 解码线程里同时跑着两个东西——QTimer 驱动的 `decodeBatch()` 和跨线程信号 `requestSeek`。它们都在同一个事件队列里排队。`decodeBatch()` 在 `push()` 里阻塞（队列满），主线程 `clear()` 唤醒它后，队列里可能已经积了新的 timer 事件，`decodeBatch()` 又跑一轮又阻塞，`requestSeek` 永远排在队尾。而播放过的位置之所以正常，是因为队列没满、`decodeBatch` 没阻塞、seek 信号能排上。

**教训：** QTimer 回调 + 跨线程信号 + 阻塞操作 = 事件队列竞争。定时器事件没有优先级，和普通信号平权，谁先排谁先跑。

**修复：** 加 `std::atomic<bool> pendingSeek`，主线程 seek 前置 `true`，`decodeBatch()` 入口和循环内检查，遇则直接 return 不阻塞。

### Bug 3: AudioPlayer 重复 new QAudioSink 不释放

**现象：** 不是 crash，但每次 seek 都 `stop()` + `start()`，旧 QAudioSink 对象堆积。

**原因：** `stop()` 只调用 `m_sink->stop()` + 置空指针，没有 `delete`。同一次播放期间多次 seek，每次创建一个新 QAudioSink 但不销毁旧的。虽然进程退出时 parent/child 机制会清，但长时间播放频繁 seek 会越堆越多。

**修复：** `stop()` 里补 `delete m_sink`；`start()` 里先判空，有旧的也删再 new。

### Bug 4: 切换视频时未清理 FFmpeg 上下文

**现象：** 双击播放列表切视频可能 crash 或花屏。

**原因：** `DecoderWorker::open()` 重开新文件时，旧的 `AVFormatContext`、`AVCodecContext`、`SwrContext`、`SwsContext` 没有被释放。直接覆盖指针导致资源泄漏，且解码器内部状态错乱。

**修复：** `open()` 开头加清理逻辑：`avformat_close_input`、`avcodec_free_context`（音视频各一个）、`swr_free`、`sws_freeContext`、`videoQueue->clear()`、重置所有状态变量。

## 关键设计

### 音视频同步：音频时钟为主

```
audioClock = 累计重采样样本数 / (采样率 / 倍速) * 1,000,000  (微秒)
```

每帧视频 pop 出来后比较 `pts_us` 和 `audioClock`：
- `pts < audioClock - 100ms` → 太晚，丢弃
- `pts > audioClock + 30ms` → 太早，缓存到 `pendingFrame`
- 否则 → 显示

Seek 后第一帧跳过"太晚"检查（`m_justSeeked` 标志），因为音频时钟跳变后刚解码的视频帧可能被误判为过期。

### 变速时重采样器重建

变速改变的是音频重采样器的输出采样率（`out_sample_rate = 原始采样率 / 倍速`），不是视频解码速度。所以变速必须设 `m_swrReady = false` 让下次音频解码时重建 `SwrContext`。视频的"倍速"只是改了主线程定时器间隔（`16ms / 倍速`），让渲染更快或更慢。

### FrameQueue 的双向阻塞

队列容量 5。push 满了生产者睡，pop 空了消费者睡，`clear()` 唤醒双方。EOF 时 `setDone()` 唤醒 pop 并返回空帧。seek 时两边都可能调用 `clear()`——主线程为了唤醒阻塞的 pop，解码线程为了清空旧帧。mutex 保护下这不会竞态。
