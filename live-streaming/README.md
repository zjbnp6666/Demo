# 直播推拉流系统

基于 Qt + FFmpeg 的多码率直播系统，一端推、一端拉。

> **目录名用 ASCII（`live-streaming`）是故意的** —— 原来的 `RTMP推流客户端和服务端` 带**中文和空格**，
> `mingw32-make` 直接报 `No rule to make target`，**别人 clone 下来根本编不了**（实测）。
> 顺带名字也改成中性的了 —— 推流端走 RTMP、拉流端走 HTTP-FLV，叫什么"RTMP…"已经不准。

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
