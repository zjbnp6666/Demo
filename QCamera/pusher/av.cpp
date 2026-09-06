#include "av.h"

AV::AV(QObject *parent)
{
    {
        lans[0].w=1280;
        lans[0].h=720;
        lans[0].bitrate=2500000;
        lans[0].fps=30;
    }
    {
        lans[1].w=640;
        lans[1].h=360;
        lans[1].bitrate=1000000;
        lans[1].fps=25;
    }
    {
        lans[2].w=320;
        lans[2].h=180;
        lans[2].bitrate=400000;
        lans[2].fps=30;
    }
    {
        lans[0].url="rtmp://127.0.0.1:1935/live/stream_720p";
        lans[1].url="rtmp://127.0.0.1:1935/live/stream_360p";
        lans[2].url="rtmp://127.0.0.1:1935/live/stream_180p";
    }
}

AV::~AV()
{
    //flush 视频:每路独立
    for(int i=0; i<3; i++){
        if(lans[i].headerWitten){
            avcodec_send_frame(lans[i].avcodec.get(), nullptr);   // 送空帧 = flush
            while(avcodec_receive_packet(lans[i].avcodec.get(), lans[i].pkt.get()) == 0){
                av_packet_rescale_ts(lans[i].pkt.get(), lans[i].avcodec->time_base,
                                     {1,1000});
                lans[i].pkt->stream_index = lans[i].vstream->index;
                av_interleaved_write_frame(lans[i].outCtx.get(), lans[i].pkt.get());
                av_packet_unref(lans[i].pkt.get());
            }
        }
    }

    // flush 音频:共用编码器 flush 一次,写三路
    if(audioInited){
        avcodec_send_frame(aucodec.get(), nullptr);
        while(avcodec_receive_packet(aucodec.get(), apkt.get()) >= 0){
            av_packet_rescale_ts(apkt.get(), aucodec->time_base, {1,1000});
            for(int i=0; i<3; i++){
                if(!lans[i].headerWitten) continue;
                AVPacket *tmp = av_packet_clone(apkt.get());
                if(!tmp) continue;
                tmp->stream_index = lans[i].ustream->index;
                av_interleaved_write_frame(lans[i].outCtx.get(), tmp);
                av_packet_free(&tmp);
            }
            av_packet_unref(apkt.get());
        }
    }

    //收尾:补尾 + 关连接(每路)
    for(int i=0; i<3; i++){
        if(lans[i].headerWitten){
            av_write_trailer(lans[i].outCtx.get());
            avio_closep(&lans[i].outCtx->pb);
        }
    }

}

void AV::run()
{
    for(int i=0;i<3;i++) this->lans[i].elap.start();
    if(!audioInited) initAudio();
    while(!isInterruptionRequested())
    {
        Nv12Frame frame=vqueue.pop();
        if(frame.width==0) {Logger::instance().error("数值异常 摄像头传值widget==0");break;}
        for(int i=0;i<3;i++)
        {

            if(!lans[i].encoderInited){
                if(lans[i].retryArmed&&lans[i].retryClock.elapsed()<3000){
                    continue;
                }
                lans[i].retryArmed = true;
                lans[i].retryClock.restart();
                initEncoder(i, frame.width, frame.height);
                if(!lans[i].encoderInited){
                    continue;// 失败,retryArmed 保持 true,退避生效
                }
                lans[i].retryArmed = false;// 成功,取消退避
            }
            qint64 now=lans[i].elap.elapsed();//记下当帧时间
            if((double)now<lans[i].nextpts) continue;//没满足时间就跳过(当前帧不满足时间)

            if(lans[i].nextpts==0) lans[i].nextpts=now;//记下第一帧时间

            const uint8_t *srcData[2]={frame.y.data(),frame.uv.data()};
            int srcStride[2]={frame.strideY,frame.strideUV};

            int retsws=sws_scale(lans[i].sws.get(), srcData,srcStride, 0, frame.height,
                                   lans[i].frame->data, lans[i].frame->linesize);
            if(retsws<0){
                Logger::instance().error(QString("第:%1 路线的视频缩放器转化失败").arg(i));
                continue;
            }

            lans[i].frame->pts=now;
            int retsend=avcodec_send_frame(lans[i].avcodec.get(),lans[i].frame.get());
            lans[i].nextpts+= 1000.0 / lans[i].fps;
            if(retsend<0&&(retsend!=AVERROR(EAGAIN)&&retsend!=AVERROR_EOF)){
                Logger::instance().error(QString("第:%1 路线的视频包异常").arg(i));
                continue;
            }
            while(avcodec_receive_packet(lans[i].avcodec.get(), lans[i].pkt.get()) == 0){
                if(!lans[i].headerWitten){
                    int retfrom=avcodec_parameters_from_context(lans[i].vstream->codecpar,
                                                                  lans[i].avcodec.get());
                    if(retfrom<0){
                        Logger::instance().error(QString("第:%1 路线的视频流数据转递异常").arg(i));
                        break;
                    }
                    lans[i].vstream->time_base = lans[i].avcodec->time_base;
                    int retwriteheader=avformat_write_header(lans[i].outCtx.get(), nullptr);
                    if(retwriteheader<0){
                        Logger::instance().error(QString("第:%1 路线的视频写头异常 尝试重新连接").arg(i));
                        resetStream(i);
                        break;
                    }
                    lans[i].headerWitten = true;
                }
                av_packet_rescale_ts(lans[i].pkt.get(), lans[i].avcodec->time_base, {1,1000});
                lans[i].pkt->stream_index = lans[i].vstream->index;
                int writeframe=av_interleaved_write_frame(lans[i].outCtx.get(), lans[i].pkt.get());
                av_packet_unref(lans[i].pkt.get());
                if(writeframe<0){
                    Logger::instance().error(QString("第:%1 路线的视频写入异常 尝试重新连接").arg(i));
                    resetStream(i);
                    break;
                }
            }
        }
        drainAudio();
    }
}

void AV::pushFrame(Nv12Frame &&frame)
{
    vqueue.push(std::move(frame));
}

void AV::stopFrameQueue()
{
    vqueue.stop();
}

void AV::appedAudio(QByteArray b)
{
    QMutexLocker locket(&amutex);
    fifo.append(b);
}

void AV::initEncoder(int i, int w, int h)
{
    if(!lans[i].outputInited){
        setupOutput(i);
        if(!lans[i].outputInited) return;
    }
    const AVCodec* vcodec=avcodec_find_encoder(AV_CODEC_ID_H264);
    if(!vcodec){
        Logger::instance().error(QString("第:%1 路线视频编码器创建异常").arg(i));
        return;
    }
    AVCodecContext*vcodecs=avcodec_alloc_context3(vcodec);
    if(!vcodecs){
        Logger::instance().error(QString("第:%1 路线的视频编码器实例创建异常").arg(i));
        return;
    }

    vcodecs->width=lans[i].w;
    vcodecs->height=lans[i].h;
    vcodecs->pix_fmt=AV_PIX_FMT_YUV420P;
    vcodecs->bit_rate=lans[i].bitrate;
    vcodecs->time_base={1,1000};
    vcodecs->framerate={lans[i].fps,1};
    vcodecs->max_b_frames=0;
    vcodecs->gop_size=lans[i].fps;

    lans[i].avcodec.reset(vcodecs);

    int retopen=avcodec_open2(lans[i].avcodec.get(),vcodec,nullptr);
    if(retopen<0){
        Logger::instance().error(QString("第:%1 路线的视频编码器打开失败").arg(i));
        return;
    }

    AVFrame *vframes=av_frame_alloc();
    if(!vframes){
        Logger::instance().error(QString("第:%1 路线的视频frame创建失败").arg(i));
        return;
    }
    vframes->width=lans[i].w;
    vframes->height=lans[i].h;
    vframes->format=AV_PIX_FMT_YUV420P;
    int retframebuf=av_frame_get_buffer(vframes,0);
    if(retframebuf<0){
        Logger::instance().error(QString("第:%1 路线的视频frame分配开失败").arg(i));
        return;
    }

    lans[i].frame.reset(vframes);

    QByteArray url=lans[i].url.toUtf8();
    int retavio=avio_open(&lans[i].outCtx->pb,url.constData(),AVIO_FLAG_WRITE);
    if(retavio<0){
        Logger::instance().error(QString("第:%1 路线的上下文打开失败").arg(i));
        return;
    }

    SwsContext *sws1 = sws_getContext(w,h, AV_PIX_FMT_NV12,      // 输入:采集 NV12
                                      lans[i].w, lans[i].h, AV_PIX_FMT_YUV420P,
                                      SWS_BILINEAR, nullptr, nullptr, nullptr);
    if(!sws1){
        Logger::instance().error(QString("第:%1 路线的视频缩放器创建失败").arg(i));
        return;
    }
    lans[i].sws.reset(sws1);

    lans[i].encoderInited=true;
}

void AV::setupOutput(int i)
{
    AVFormatContext *outCtx=nullptr;
    QByteArray url = lans[i].url.toUtf8();
    int retCtx=avformat_alloc_output_context2(&outCtx,nullptr,"flv",url.constData());
    if(retCtx<0){
        Logger::instance().error(QString("第:%1 路线路的上下文创建异常了").arg(i));
        return;
    }


    lans[i].vstream=avformat_new_stream(outCtx,nullptr);
    if(!lans[i].vstream){
        Logger::instance().error(QString("第:%1 路线的视频流创建异常").arg(i));
        return;
    }


    lans[i].ustream=avformat_new_stream(outCtx,nullptr);
    if(!lans[i].ustream){
        Logger::instance().error(QString("第:%1 路线的音频流创建异常").arg(i));
        return;
    }

    AVCodecContext *ucodecper=aucodec.get();
    lans[i].ustream->time_base = aucodec->time_base;
    int retuStream=avcodec_parameters_from_context(lans[i].ustream->codecpar,ucodecper);
    if(retuStream<0){
        Logger::instance().error(QString("第:%1 路线的音频流数据传递异常").arg(i));
        return;
    }
    AVPacket *pkt1=av_packet_alloc();
    if(!pkt1){
        Logger::instance().error(QString("第:%1 路线的pkt创建异常").arg(i));
        return;
    }

    lans[i].pkt.reset(pkt1);

    lans[i].outCtx.reset(outCtx);

    lans[i].outputInited=true;
}

void AV::resetStream(int i)
{
    //重连=重建这整条流 凡是第一次启动时这条路要分配时的东西 重连时必须重新分配
    if(lans[i].headerWitten) av_write_trailer(lans[i].outCtx.get());//如果写了头 就把尾也写了
    avio_closep(&lans[i].outCtx->pb);          // 关这一路的连接
    lans[i].outCtx.reset();//必须调用avforamt_free_context析构 不然即使重连 也会导致连接旧的上下文
    lans[i].vstream=nullptr;
    lans[i].ustream=nullptr;
    lans[i].avcodec.reset();                    // 视频编码器
    lans[i].sws.reset();                        // 缩放器
    lans[i].frame.reset();                      // 视频 frame
    lans[i].encoderInited = false;              // 复位,下一帧触发 initEncoder 重连
    lans[i].headerWitten  = false;
    lans[i].outputInited=false;
    lans[i].nextpts=0;
    lans[i].elap.restart();
}

void AV::drainAudio()
{
    while(true)                        // ← 循环，把所有攒够的块都处理掉
    {
        QByteArray byt;
        {
            QMutexLocker locker(&amutex);   // 锁只包住取数据这一下
            if(fifo.size() < 4096) break;   // 不够一块，收工
            byt = fifo.mid(0, 4096);
            fifo.remove(0, 4096);
        }                                   // ←锁立刻放

        // 锁外：拿 byt 编码、写三路（卡多久都不影响主线程）
        const uint8_t *in[1] = { reinterpret_cast<const
                                                 uint8_t*>(byt.constData()) };
        int retmake = av_frame_make_writable(uframe.get());
        if(retmake < 0){ Logger::instance().error("音频包framemake失败");
            continue; }   // 数据已消费，continue 安全
        int retswr = swr_convert(swr.get(), uframe->data, 1024, in, 1024);
        if(retswr < 0) continue;
        uframe->pts = aPts; aPts += 1024;
        int retsend = avcodec_send_frame(aucodec.get(), uframe.get());
        if(retsend < 0 && retsend != AVERROR(EAGAIN) && retsend !=
                                                             AVERROR_EOF) continue;
        while(avcodec_receive_packet(aucodec.get(), apkt.get()) >= 0){
            av_packet_rescale_ts(apkt.get(), aucodec->time_base, {1,1000});
            for(int i=0;i<3;i++){
                if(!lans[i].headerWitten) continue;
                AVPacket *tmp = av_packet_clone(apkt.get());
                if(!tmp) continue;
                tmp->stream_index = lans[i].ustream->index;
                int retwrite =
                    av_interleaved_write_frame(lans[i].outCtx.get(), tmp);
                av_packet_free(&tmp);
                if(retwrite<0){ resetStream(i);
                    Logger::instance().error(QString("第:%1 路线的音频包写入异常").arg(i));
                    continue; }
            }
            av_packet_unref(apkt.get());
        }
    }   // ←回到 while，看 fifo 还有没有攒够的块
}

void AV::initAudio()
{
    const AVCodec* ucodec=avcodec_find_encoder(AV_CODEC_ID_AAC);
    if(!ucodec){
        Logger::instance().error("音频AAC编码器创建异常");
        return;
    }
    AVCodecContext* ucodecs=nullptr;
    ucodecs=avcodec_alloc_context3(ucodec);
    if(!ucodecs){
        Logger::instance().error("音频编码器实例创建异常");
        return ;
    }
    ucodecs->sample_rate=44100;
    av_channel_layout_from_mask(&ucodecs->ch_layout, AV_CH_LAYOUT_STEREO);
    ucodecs->sample_fmt=AV_SAMPLE_FMT_FLTP;
    ucodecs->bit_rate=128000;
    ucodecs->time_base={1,44100};

    aucodec.reset(ucodecs);

    int retopen=avcodec_open2(aucodec.get(),ucodec,nullptr);
    if(retopen<0){
        Logger::instance().error("音频编码器打开失败");
        return;
    }

    AVFrame *uframes=av_frame_alloc();
    if(!uframes){
        Logger::instance().error("音频frame创建失败");
        return;
    }
    uframes->sample_rate=44100;
    av_channel_layout_from_mask(&uframes->ch_layout, AV_CH_LAYOUT_STEREO);
    uframes->format=AV_SAMPLE_FMT_FLTP;
    uframes->nb_samples=1024;
    int retframebuf=av_frame_get_buffer(uframes,0);
    if(retframebuf<0){
        Logger::instance().error("音频frame分配内存失败哦");
        return;
    }

    uframe.reset(uframes);

    SwrContext *swr1 = swr_alloc();
    if(!swr1){
        Logger::instance().error("音频转换器创建失败");
        return;
    }
    av_opt_set_int(swr1, "in_sample_fmt",  AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int(swr1, "in_sample_rate", uframe->sample_rate, 0);
    av_opt_set_chlayout(swr1, "in_chlayout", &uframe->ch_layout, 0);
    av_opt_set_int(swr1, "out_sample_fmt",  AV_SAMPLE_FMT_FLTP, 0);
    av_opt_set_int(swr1, "out_sample_rate", uframe->sample_rate, 0);
    av_opt_set_chlayout(swr1, "out_chlayout", &uframe->ch_layout, 0);
    int retint=swr_init(swr1);
    if(retint<0){
        Logger::instance().error("音频转换器工作失败");
        return;
    }

    swr.reset(swr1);

    apkt.reset(av_packet_alloc());

    audioInited=true;
}
