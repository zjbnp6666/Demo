#ifndef STREAMLANE_H
#define STREAMLANE_H
#include <QString>
#include <QElapsedTimer>
#include <memory>
extern "C"{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}
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

    double nextpts=0;//动态阈值
};
#endif // STREAMLANE_H
