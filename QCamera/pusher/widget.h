#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include<QPainter>
#include<QVideoFrame>
#include"camera.h"
#include<QImage>
#include<QElapsedTimer>
extern "C"{
#include<libavcodec/avcodec.h>
#include<libswscale/swscale.h>
#include<libswresample/swresample.h>
#include<libavformat/avformat.h>
#include<libavutil/opt.h>
}

#include<qdebug.h>
#include"audioformat.h"
#include<QMessageBox>
#include<QElapsedTimer>
#include<array>
struct StreamLane
{
    int w,h,fps,bitrate;
    QString url;
    static void freeavcodec(AVCodecContext *codec){
        if(codec) avcodec_free_context(&codec);
    }
    using Codec=std::unique_ptr<AVCodecContext,void(*)(AVCodecContext*)>;
    Codec avcodec={nullptr,freeavcodec};

    static void freeSWS(SwsContext *sws){
        if(sws) sws_free_context(&sws);
    }
    //Frame创建
    static void freeframe(AVFrame *frame){
        if(frame) av_frame_free(&frame);
    }
    using Frames=std::unique_ptr<AVFrame,void(*)(AVFrame *)>;
    Frames frame{nullptr,freeframe};

    using SWS=std::unique_ptr<SwsContext,void(*)(SwsContext*)>;
    SWS sws{nullptr,freeSWS};

    //创建输出上下文
    static void freeAV(AVFormatContext *outCtx){
        if(outCtx) avformat_free_context(outCtx);
    }
    using AVFormat=std::unique_ptr<AVFormatContext,void(*)(AVFormatContext*)>;
    AVFormat outCtx{nullptr,freeAV};

    AVStream *vstream=nullptr;
    AVStream *ustream=nullptr;

    static void freePkt(AVPacket *p) {
        if(p) av_packet_free(&p);
    }
    using Pkt = std::unique_ptr<AVPacket, void(*)(AVPacket*)>;
    Pkt pkt{nullptr, freePkt};

    QElapsedTimer elap;
    bool headerWitten = false;
    bool encoderInited = false;
    bool outputInited = false;

    QElapsedTimer retryClock;
    bool retryArmed = false;

    int frameCounter = 0;

    double nextpts=0;
};
QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;
    void paintEvent(QPaintEvent *event) override;

private:
    Ui::Widget *ui;
    void extracted(int &retsend);
    void upVideo(QVideoFrame frame);
    void initEncoder(int i,int w,int h);
    void setupAudio();
    void setupOutput(int i);
    std::unique_ptr<Camera> camera;
    QImage currentImage;

    //音频转化器
    static void freeFormatCtxs(SwrContext *swr){
        if(swr) swr_free(&swr);
    }
    using SWR=std::unique_ptr<SwrContext,void(*)(SwrContext*)>;
    SWR swr{nullptr,freeFormatCtxs};


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

    //创建麦克风
    std::unique_ptr<AudioFormat> au;
    //麦克风缓冲区
    QByteArray fifo;
    void onAuReadAll();

    //写帧失败 或者 断链的重连操作
    void resetStream(int i);

    std::array<StreamLane,3> lans;

    bool audioInited=false;

    qint64 aPts=0;
};
#endif // WIDGET_H
