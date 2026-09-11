#ifndef AV_H
#define AV_H

#include <QObject>
#include<QThread>
#include"streamlane.h"
#include"framequeue.h"
#include"logger.h"
extern "C"{
#include<libavcodec/avcodec.h>
#include<libswscale/swscale.h>
#include<libswresample/swresample.h>
#include<libavformat/avformat.h>
#include<libavutil/opt.h>
#include<libavutil/error.h>
}
class AV : public QThread
{
    Q_OBJECT
public:
    explicit AV(QObject *parent = nullptr);
    ~AV();
    void run() override;
    void pushFrame(Nv12Frame &&frame);
    void stopFrameQueue();
    void appedAudio(QByteArray b);
private:
    void initEncoder(int i,int w,int h);
    void setupOutput(int i);
    void resetStream(int i);
    std::array<StreamLane, 3> lans;
    FrameQueue vqueue;

    //编码器的创建
    static void freeavcodec(AVCodecContext *codec){
        if(codec) avcodec_free_context(&codec);
    }
    using Codec=std::unique_ptr<AVCodecContext,void(*)(AVCodecContext*)>;
    Codec aucodec={nullptr,freeavcodec};
    //Frame创建
    static void freeframe(AVFrame *frame){
        if(frame) av_frame_free(&frame);
    }
    using Frames=std::unique_ptr<AVFrame,void(*)(AVFrame *)>;
    Frames uframe{nullptr,freeframe};

    //音频包
    static void freePkt(AVPacket *p) {
        if(p) av_packet_free(&p);
    }
    using Pkt = std::unique_ptr<AVPacket, void(*)(AVPacket*)>;
    Pkt apkt{nullptr, freePkt};

    static void freeswr(SwrContext *swr){
        if(swr) swr_free(&swr);
    }
    using SWR=std::unique_ptr<SwrContext,void(*)(SwrContext*)>;
    SWR swr{nullptr,freeswr};

    //关于音频
    QByteArray fifo;
    qint64 aPts=0;
    QMutex amutex;
    bool audioInited=false;
    void drainAudio();
    void initAudio();
signals:
};

#endif // AV_H
