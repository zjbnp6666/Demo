# Demo —— 个人项目合集

C++ / Qt / FFmpeg 方向的项目合集。

## 项目列表

| 项目 | 说明 |
|------|------|
| [QTffmpeg-Demo](QTffmpeg-Demo/) | Qt6 + FFmpeg 视频播放器：音视频同步、倍速、进度条 seek、播放列表 |
| [直播推拉流系统](live-streaming/) | 摄像头直播推拉流：推流端(三路 H264/AAC，走 RTMP) + 拉流端(走 HTTP-FLV，音视频同步、坏时钟护栏、清晰度切换) |

## 技术栈

- C++17
- Qt 6（Widgets / Multimedia）
- FFmpeg（avcodec / avformat / swscale / swresample）

## 说明

每个项目是独立的 CMake 工程，各自目录里有自己的 README，点进去看详细介绍。
