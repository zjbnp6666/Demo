#include "avdeio.h"
#include<QDebug>
#include<QByteArray>

Avdeio::Avdeio(const QString &url, QObject *parent)
    : QThread{parent}
{
    m_Queue=new FrameQueue;
    m_url=url;
    t.start();
}

void Avdeio::run()
{

    while(!isInterruptionRequested()){
        if(!init()){
            emit connectFail();
            if (m_url != DEFAULT_URL) {
                m_url = DEFAULT_URL;
                Logger::instance().info("已切换到默认地址");
            }
            if (isInterruptionRequested()) break;
            msleep(2000);
            continue;
        }
        while(!isInterruptionRequested()){
            int ret=av_read_frame(outCtx.get(),pkt.get());
            m_lastReadMs=nowMs();     // 看门狗：刷新"最后一次成功读到包的时刻"
            if(ret<0)
            {
                char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));

                Logger::instance().warn(QString("断开连接了,准备重连:%1").arg(errbuf));
                emit reconnecting();
                cleanup();
                break;
            }
            m_gotFirstPacket=true;
            if(pkt->stream_index==AVDEOIDX)
            {
                int ret=avcodec_send_packet(avcodec.get(),pkt.get());
                if(ret<0&&ret!=AVERROR(EAGAIN))
                {
                    Logger::instance().warn("有个包坏掉了");
                    av_packet_unref(pkt.get());
                    continue;
                }
                av_packet_unref(pkt.get());
                while(1){
                    int ret=avcodec_receive_frame(avcodec.get(),frame.get());
                    if(ret==AVERROR(EAGAIN)||ret==AVERROR_EOF) break;
                    if(ret<0){
                        Logger::instance().warn("视频解码出错");
                        break;
                    }
                    if(!swsnew){
                        SwsContext *s=sws_getContext(frame->width,frame->height,(AVPixelFormat)frame->format,
                                                       frame->width,frame->height,AV_PIX_FMT_RGB24,SWS_BILINEAR,nullptr,nullptr,nullptr);
                        sws.reset(s);
                        swsnew=true;
                        m_rgbBuff.resize((size_t)frame->width*frame->height*3);
                        rgbStride = frame->width * 3;
                    }

                    uint8_t *rgbData=m_rgbBuff.data();
                    // 5.3 YUV → RGB
                    sws_scale(sws.get(), frame->data, frame->linesize, 0, frame->height,
                              &rgbData, &rgbStride);
                    QImage image(m_rgbBuff.data(),frame->width,frame->height,QImage::Format_RGB888);

                    FrameData data;
                    data.pts_us = frame->pts * av_q2d(m_videoTimeBase) * 1000000.0;
                    data.image  = image.copy();
                    m_Queue->push(std::move(data));

                }
            }
            else if(pkt->stream_index==AUDEOIDX)
            {
                int ret=avcodec_send_packet(aucodec.get(),pkt.get());
                if(ret<0&&ret!=AVERROR(EAGAIN))
                {
                    Logger::instance().warn("有个包坏掉了");
                    av_packet_unref(pkt.get());
                    continue;
                }
                av_packet_unref(pkt.get());
                while(1){
                    int ret=avcodec_receive_frame(aucodec.get(),aframe.get());
                    if(ret==AVERROR(EAGAIN)||ret==AVERROR_EOF) break;
                    if(ret<0){
                        Logger::instance().warn("音频解码出错");
                        break;
                    }
                    if(!swrnew)
                    {
                        SwrContext *swr1=swr_alloc();
                        av_opt_set_int(swr1, "in_sample_fmt",  aframe->format, 0);
                        av_opt_set_int(swr1, "in_sample_rate", aframe->sample_rate, 0);
                        av_opt_set_chlayout(swr1,"in_chlayout", &aframe->ch_layout, 0);
                        av_opt_set_int(swr1, "out_sample_fmt",  AV_SAMPLE_FMT_S16, 0);
                        av_opt_set_int(swr1, "out_sample_rate", aframe->sample_rate, 0);
                        av_opt_set_chlayout(swr1, "out_chlayout", &aframe->ch_layout, 0);
                        swr_init(swr1);
                        swr.reset(swr1);
                        swrnew=true;
                    }
                    int outBytes = aframe->nb_samples * aframe->ch_layout.nb_channels * 2;
                    std::vector<uint8_t> outBuf(outBytes);
                    uint8_t *outPtr = outBuf.data();

                    int outSamples=swr_convert(swr.get(), &outPtr, aframe->nb_samples,
                                                 (const uint8_t **)aframe->data, aframe->nb_samples);
                    AudioData data;
                    data.data=QByteArray((const char*)outBuf.data(),outSamples*aframe->ch_layout.nb_channels*2);
                    data.pts_us=aframe->pts*av_q2d(m_audioTimeBase)*1000000.0;
                    m_Queue->push1(data);
                }
            }
        }
    }
}

FrameData Avdeio::popFrame()
{
    return m_Queue->pop();
}

bool Avdeio::init()
{
    m_lastReadMs = nowMs();   // 每次连接前刷新，避免 callback 因"上次连接留下的过期值"锁死 open
    m_gotFirstPacket=false;
    if(!newWin){
        AVFormatContext *ctx = avformat_alloc_context();
        ctx->interrupt_callback={interruptCb,this};
        QByteArray urlBytes = m_url.toUtf8();
        AVDictionary *opts = nullptr;
        av_dict_set(&opts, "rw_timeout", "3000000", 0);   // 3 秒（微秒
        av_dict_set(&opts, "probesize", "1024", 0);        // 探测字节压到最小 → 不缓冲等探测
        av_dict_set(&opts, "analyzeduration", "100000", 0); // 探测时长压到 50ms
        //av_dict_set(&opts, "fflags", "nobuffer", 0);       // 关 avformat 输入缓冲，读一帧推一帧
        int temp=avformat_open_input(&ctx, urlBytes.constData(), nullptr, &opts);
        av_dict_free(&opts);
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(temp, errbuf, sizeof(errbuf));
        if(temp<0)
        {
            Logger::instance().error(QString("open失败: %1").arg(errbuf));
            return false;
        }
        int ret=avformat_find_stream_info(ctx,nullptr);
        if(ret<0){
            char errbuf1[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, errbuf1, sizeof(errbuf1));
            Logger::instance().error(QString("find失败了:%1").arg(errbuf1));
            avformat_close_input(&ctx);
            return false;
        }
        outCtx.reset(ctx);
        Logger::instance().info("连接成功");
        for(int i=0;i<outCtx->nb_streams;i++)
        {
            AVStream *stream=outCtx->streams[i];
            AVCodecParameters *cp=stream->codecpar;
            if(cp->codec_type==AVMEDIA_TYPE_AUDIO){
                AUDEOIDX=i;
                const AVCodec*ucodec=avcodec_find_decoder(cp->codec_id);
                if(!ucodec){
                    Logger::instance().error("不支持的音频解码器");
                    return false;
                }
                AVCodecContext *codec1=avcodec_alloc_context3(ucodec);
                avcodec_parameters_to_context(codec1,cp);
                aucodec.reset(codec1);
                int ret1=avcodec_open2(aucodec.get(),ucodec,nullptr);
                if(ret1<0)
                {
                    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
                    av_strerror(ret1, buf, sizeof(buf));
                    Logger::instance().error(QString("音频解码器打开失败: %1").arg(buf));
                    return false;
                }
                m_audioTimeBase=stream->time_base;
            }
            if(cp->codec_type==AVMEDIA_TYPE_VIDEO){
                AVDEOIDX=i;
                const AVCodec* vcodec=avcodec_find_decoder(cp->codec_id);
                if(!vcodec){
                    Logger::instance().error("不支持的视频解码器");
                    return false;   // 让 run() 重连
                }
                AVCodecContext *codec=avcodec_alloc_context3(vcodec);
                avcodec_parameters_to_context(codec,cp);
                avcodec.reset(codec);
                int ret2=avcodec_open2(avcodec.get(),vcodec,nullptr);
                if(ret2<0)
                {
                    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
                    av_strerror(ret2, buf, sizeof(buf));
                    Logger::instance().error(QString("视频解码器打开失败: %1").arg(buf));
                    return false;
                }
                m_videoTimeBase=stream->time_base;
            }
        }
        // 探针：把流的构成打出来（HTTP-FLV 会多一条 data 流，看 index 对不对）
        {
            QString s = QString("【流】nb_streams=%1  AVIDX=%2 UIDX=%3  各流:")
                        .arg(outCtx->nb_streams).arg(AVDEOIDX).arg(AUDEOIDX);
            for(int i=0;i<outCtx->nb_streams;i++)
                s += QString(" [%1:%2]").arg(i).arg(outCtx->streams[i]->codecpar->codec_type);
            Logger::instance().info(s);
        }

        AVPacket *pk1=av_packet_alloc();
        pkt.reset(pk1);
        AVFrame *fr1=av_frame_alloc();
        frame.reset(fr1);
        AVFrame *fr2=av_frame_alloc();
        aframe.reset(fr2);
        newWin=true;
    }
    return true;
}

void Avdeio::cleanup()
{
    outCtx.reset();
    avcodec.reset();
    aucodec.reset();
    pkt.reset();
    swr.reset();
    sws.reset();
    aframe.reset();
    frame.reset();
    newWin=false;
    swsnew=false;
    swrnew=false;
    t.restart();
}

int Avdeio::interruptCb(void *opaque)
{

    auto *self=static_cast<Avdeio*>(opaque);

    // ① 用户主动要求退出（析构里的 requestInterruption）—— 永远放第一条
    if(self->isInterruptionRequested()) return 1;

    // ② 哨兵：还没读到第一个包 → 不管。刚连上、流还没推上来，等多久都算正常
    if(!self->m_gotFirstPacket) return 0;

    // ③ 3 秒看门狗：已经在播了，却超过 3 秒没读到包 → 报中断
    qint64 now=self->nowMs();
    if(self->m_lastReadMs && now - self->m_lastReadMs > 3000) return 1;
    return 0;

}
// ⚠️ 上面的看门狗在 HTTP-FLV 下有效（实测挂起服务器 ~4 秒就断），
//    但【在 rtmp 下不生效】—— 实测挂起服务器 20 秒，本回调【一次都没被调用】
//    （主线程逐秒采样：距上次回调 和 距上次读 完全相等，一路涨到 29 秒）。
//    原因未查清；源码上这条链每环都通，但实测就是不生效。详见 README「踩坑记录」。

qint64 Avdeio::nowMs()
{
    if(t.isValid()){
        return t.elapsed();
    }
    return 0;
}

Avdeio::~Avdeio()
{
    requestInterruption();
    wait();
    delete m_Queue;
}


