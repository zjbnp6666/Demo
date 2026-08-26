# QCamera —— 摄像头直播推拉流系统

基于 Qt + FFmpeg 的多码率直播系统，一端推、一端拉。

## 组成

| 目录 | 说明 |
|------|------|
| [pusher](pusher/) | 推流端：摄像头 + 麦克风采集，三路 H264/AAC 推 RTMP |
| [client](client/) | 拉流端：RTMP 拉流播放，音视频同步 + 清晰度切换 |

## 数据流

```
摄像头/麦克风 → pusher 推三档清晰度(720p/360p/180p) → RTMP 服务器 → client 拉流按清晰度切换播放
```

## 构建

两端是独立的 CMake 工程，各自在 `pusher/` 和 `client/` 目录里，各自的 README 有详细构建说明。
