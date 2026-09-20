# RTMP 推流客户端和服务端

基于 Qt + FFmpeg 的多码率直播系统，一端推、一端拉。

## 组成

| 目录 | 说明 |
|------|------|
| [push](push/) | 推流端：摄像头 + 麦克风采集，三路 H264/AAC（720p/360p/180p）推 RTMP |
| [client](client/) | 拉流端：**默认走 HTTP-FLV** 播放，音视频同步 + 坏时钟护栏 + 清晰度切换 |

## 数据流

```
摄像头/麦克风 → push 推三档清晰度(720p/360p/180p) → RTMP 服务器
                                                      ├─→ client 按清晰度切换播放（HTTP-FLV）
                                                      └─→ 浏览器网页播放器
```

> **推流走 RTMP、拉流默认走 HTTP-FLV** —— 不是为了追新，是实测出来的：
> **rtmp 下客户端检测不到服务器静默断网**（钩子一次都不跳、`rw_timeout` 也不生效），
> http-flv 下 ~3-4 秒就能自己重连。**对照实验和取舍写在 [`client/README.md`](client/README.md) 的「踩坑记录」里。**

## 构建

两端是独立的 CMake 工程，各自在 `push/` 和 `client/` 目录里，各自的 README 有详细构建说明。

两端都依赖 MSYS2 的 FFmpeg + Qt6，**必须用 MSYS2 kit 编译**（GCC 版本不匹配会启动即崩）。
