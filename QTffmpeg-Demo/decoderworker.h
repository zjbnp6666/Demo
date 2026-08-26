#ifndef DECODERWORKER_H
#define DECODERWORKER_H

#include <QObject>
#include <QTimer>
#include <QByteArray>
#include <atomic>
#include "framequeue.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}
class FrameQueue;

class DecoderWorker : public QObject
{
    Q_OBJECT
public:
    explicit DecoderWorker(FrameQueue *vq, QObject *parent = nullptr);
    ~DecoderWorker() override;

    // 主线程只读，解码线程写入（无需锁）
    long long audioClock = 0;

    // seek 期间阻止 decodeBatch 继续推帧（原子变量，跨线程安全）
    std::atomic<bool> pendingSeek{false};

public slots:
    void open(const QString &filepath);
    void stop();
    void seek(int64_t targetUs);
    void setSpeed(double speed);

signals:
    void audioReady(QByteArray pcm, int channels, int sampleRate);
    void durationReady(double seconds);
    void openFailed(const QString &msg);
    void closeFrame();
    void isSeekFalse();

private:
    void decodeBatch();
    void decodeOneVideoFrame();     // seek 后解码一帧视频，同步 audioClock

    // ---- FFmpeg（RAII 智能指针管理） ----
    static void freeFormatCtx(AVFormatContext *ctx) { if (ctx) avformat_close_input(&ctx); }
    static void freeCodecCtx(AVCodecContext *c)     { if (c)   avcodec_free_context(&c);  }

    using FmtPtr  = std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)>;
    using CdcPtr  = std::unique_ptr<AVCodecContext,  void(*)(AVCodecContext*)>;

    FmtPtr m_formatCtx     {nullptr, freeFormatCtx};
    CdcPtr m_videoCodecCtx {nullptr, freeCodecCtx};
    CdcPtr m_audioCodecCtx {nullptr, freeCodecCtx};

    AVPacket   *m_packet   = nullptr;
    AVFrame    *m_frame    = nullptr;
    SwrContext *m_swr      = nullptr;
    SwsContext *m_sws      = nullptr;
    bool        m_swsReady = false;
    bool        m_swrReady = false;
    AVRational  m_videoTimeBase;



    int     m_videoStreamIndex  = -1;
    int     m_audioStreamIndex  = -1;
    int     m_audioSampleRate   = 0;
    double  m_frameDelayMs      = 0;   // 每帧间隔（毫秒）
    double  m_totalDuration     = 0;   // 秒
    double  m_speed             = 1.0;

    long long m_audioSamplesWritten = 0; // 累计重采样输出样本数

    FrameQueue *m_videoQueue = nullptr;
    QTimer     *m_timer      = nullptr;
};

#endif // DECODERWORKER_H
