#ifndef AVDEIO_H
#define AVDEIO_H

#include <QObject>
#include <QString>
extern "C"{
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libswresample/swresample.h>
#include<libswscale/swscale.h>
#include<libavutil/opt.h>
}
#include<QThread>
#include<QImage>
#include"tqueue.h"
#include"logger.h"
#include<QElapsedTimer>
class Avdeio : public QThread
{
    Q_OBJECT
public:
    explicit Avdeio(const QString& url,QObject *parent = nullptr);
    ~Avdeio();
    void run();
    FrameData popFrame();
    FrameQueue *m_Queue = nullptr;
private:
    //转化器的创建
    static void freeFormatCtx(SwsContext *sws){
        if(sws) sws_free_context(&sws);
    }
    using SWS=std::unique_ptr<SwsContext,void(*)(SwsContext*)>;
    SWS sws{nullptr,freeFormatCtx};
    bool swsnew=false;

    static void freerFormatCtx(SwrContext *swr){
        if(swr) swr_free(&swr);
    }
    using SWR=std::unique_ptr<SwrContext,void(*)(SwrContext*)>;
    SWR swr{nullptr,freerFormatCtx};
    bool swrnew=false;
    //解码器的创建
    static void freeavcodec(AVCodecContext *codec){
        if(codec) avcodec_free_context(&codec);
    }
    using Codec=std::unique_ptr<AVCodecContext,void(*)(AVCodecContext*)>;
    Codec avcodec={nullptr,freeavcodec};
    Codec aucodec={nullptr,freeavcodec};

    //Frame创建
    static void freeframe(AVFrame *frame){
        if(frame) av_frame_free(&frame);
    }
    using Frames=std::unique_ptr<AVFrame,void(*)(AVFrame *)>;
    Frames frame{nullptr,freeframe};
    Frames aframe{nullptr,freeframe};

    //创建输出上下文
    static void freeAV(AVFormatContext *outCtx){
        if(outCtx) avformat_close_input(&outCtx);
    }
    using AVFormat=std::unique_ptr<AVFormatContext,void(*)(AVFormatContext*)>;
    AVFormat outCtx{nullptr,freeAV};

    //创建流
    AVStream *vStream = nullptr;
    AVStream *uStream = nullptr;

    //pkt
    static void freePkt(AVPacket *p) {
        if(p) av_packet_free(&p);
    }
    using Pkt = std::unique_ptr<AVPacket, void(*)(AVPacket*)>;
    Pkt pkt{nullptr, freePkt};

    std::vector<uint8_t> m_rgbBuff;

    int rgbStride=0;//每行像素字节数

    int AVDEOIDX=-1;
    int AUDEOIDX=-1;
    AVRational  m_videoTimeBase;
    AVRational  m_audioTimeBase;

    bool newWin=false;//第一次创建解码器等
    bool init();//第一次创建

    QString m_url; //URL 连接推流服务器地址

    void  cleanup();// 释放解码器/转换器/上下文

    const QString DEFAULT_URL = "rtmp://127.0.0.1:1935/live/stream";

    QElapsedTimer t;
signals:
    void connectFail();//连不上发送信号
    void reconnecting();//断流了
};

#endif // AVDEIO_H
