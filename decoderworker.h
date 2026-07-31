#ifndef DECODERWORKER_H
#define DECODERWORKER_H

#include <QObject>
#include <QTimer>
#include <QByteArray>
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

    // 主线程只读（解码线程写，无需锁）
    long long audioClock = 0;

public slots:
    void open(const QString &filepath);
    void stop();
    void seek(int64_t targetUs);
    void decodeBatch();
    void start();
    void setSpeed(double seep);
signals:
    void audioReady(QByteArray pcm, int channels, int sampleRate);
    void durationReady(double seconds);
    void openFailed(const QString &msg);
    void closeFrame();

private:
    FrameQueue *videoQueue;
    QTimer *m_timer = nullptr;

    AVFormatContext *Ctx = nullptr;
    int AVSTREAM = -1;
    int AUSTREAM = -1;
    AVCodecContext *avcodec = nullptr;
    AVCodecContext *aucodec = nullptr;
    AVPacket *pkt = nullptr;
    AVFrame  *frame = nullptr;
    SwrContext *swr = nullptr;
    SwsContext *sws = nullptr;
    bool swrnew = false;
    bool swsnew = false;
    AVRational videoTimeBase;
    long long totalWritten = 0;
    void decodeOneVideoFrame();
    double totaSUM=0;
    double speed=1.0;
    int m_audioSampleRate=0;
    double m_baseFrameDelay = 0;

};

#endif // DECODERWORKER_H
