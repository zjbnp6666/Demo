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

两个值得说的死锁问题：

### Bug 1: seek 后死锁（decodeOneVideoFrame 一直读到 EOF）

**现象：** 拖动进度条后画面卡死。

**原因：** `decodeOneVideoFrame()` 内层 `while(receive_frame)` 用 `break` 退出，但外层 `while(av_read_frame)` 没退出，一直读到文件末尾。帧队列满 → `push()` 阻塞 → `isSeekFalse` 信号发不出 → `strat()` 不消费 → 死锁。

**修复：** `break` → `return`，解码一帧后立即返回。

### Bug 2: seek 信号被 QTimer 事件挤掉

**现象：** 直接拖到远处卡死，但拖到已播放过的位置正常。

**原因：** `decodeBatch()` 在 `push()` 里阻塞，`requestSeek` 信号排队在解码线程事件队列里。`clear()` 唤醒 `push()` 后，积压的 QTimer 事件又触发新一轮 `decodeBatch()` 阻塞，`requestSeek` 永远排不上队。

**修复：** 加 `std::atomic<bool> pendingSeek`，seek 前置位，`decodeBatch()` 入口和循环内检查，遇则退出。
