#include "decoderworker.h"
#include <qdebug.h>
DecoderWorker::DecoderWorker(FrameQueue *vq, QObject *parent)
    :QObject(parent)
{
    videoQueue=vq;
    pkt=av_packet_alloc();
    frame=av_frame_alloc();
    m_timer=new QTimer(this);
    connect(m_timer,&QTimer::timeout,this,&DecoderWorker::decodeBatch);
}

DecoderWorker::~DecoderWorker()
{
    m_timer->stop();
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&avcodec);
    avcodec_free_context(&aucodec);
    swr_free(&swr);
    sws_freeContext(sws);
    avformat_free_context(Ctx);
}

void DecoderWorker::open(const QString &filepath)
{
    if(Ctx){
        avformat_close_input(&Ctx);
    avcodec_free_context(&avcodec);      // 清视频解码器残留
    avcodec_free_context(&aucodec);      // 清音频解码器残留
    videoQueue->clear();
    swr_free(&swr);
    sws_freeContext(sws);
    sws=nullptr;
    totalWritten=0;
    audioClock=0;
    AVSTREAM = -1;
    AUSTREAM = -1;
    }
    QByteArray mmp=filepath.toLocal8Bit();
    const char *mp4=mmp.constData();
    // ---- 打开文件 ----
    if (avformat_open_input(&Ctx,mp4, nullptr, nullptr)) {
        // avformat_open_input: 打开文件，读取头信息，填充 AVFormatContext
        emit openFailed("文件打开失败");
        return;
    }
    avformat_find_stream_info(Ctx, nullptr);
    // 上面这句会扫描几秒，探测流的编码参数(分辨率/采样率等)，之后才能用

    // ---- 遍历音视频流，初始化解码器 ----
    // Ctx->duration: 容器总时长，单位 AV_TIME_BASE=1/1000000秒
    totaSUM=(double)Ctx->duration/AV_TIME_BASE;
    emit durationReady(totaSUM);
    double frameDelay;
    for (int i = 0; i < Ctx->nb_streams; i++) {
        AVStream *stream = Ctx->streams[i];
        AVCodecParameters *codec = stream->codecpar;
        // codecpar: 编码参数（格式/分辨率/采样率等），不依赖解码器就能读

        if (codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            AUSTREAM = i;
            const AVCodec *cc = avcodec_find_decoder(codec->codec_id);
            // avcodec_find_decoder: 根据 codec_id 查找对应解码器(如 AAC→aac解码器)
            aucodec = avcodec_alloc_context3(cc);
            // avcodec_alloc_context3: 用指定解码器创建解码上下文
            avcodec_parameters_to_context(aucodec, codec);
            // 把 codecpar 里的参数拷进解码上下文
            avcodec_open2(aucodec, cc, nullptr);
            // 打开解码器，准备解码
            m_audioSampleRate = aucodec->sample_rate;
        }

        if (codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            const AVCodec *cc = avcodec_find_decoder(codec->codec_id);
            avcodec = avcodec_alloc_context3(cc);
            avcodec_parameters_to_context(avcodec, codec);
            avcodec_open2(avcodec, cc, nullptr);
            AVSTREAM = i;

            // avg_frame_rate: 平均帧率的分数表示（如 30000/1001=29.97fps）
            frameDelay = (double)stream->avg_frame_rate.den
                         / stream->avg_frame_rate.num * 1000;  // 帧间隔(毫秒)
            videoTimeBase = stream->time_base;
            // time_base: PTS 的计时单位（如 1/90000 秒 → 1个PTS单位=1/90000秒）
            m_baseFrameDelay = frameDelay;
        }
    }
    m_timer->setInterval((int)(frameDelay/speed));
    m_timer->setSingleShot(false);
    m_timer->start();
    swsnew=false;
    swrnew=false;
}

void DecoderWorker::stop()
{
    m_timer->stop();
}

void DecoderWorker::seek(int64_t targetUs)
{

    if(targetUs==-1)
    {
        videoQueue->setGO();
        m_timer->start();
        targetUs=0;
    }
    av_seek_frame(Ctx, -1, targetUs, AVSEEK_FLAG_BACKWARD);
    // -1: 时间单位用 AV_TIME_BASE（微秒）
    // AVSEEK_FLAG_BACKWARD: 跳到目标时间之前最近的关键帧

    avcodec_flush_buffers(avcodec);      // 清视频解码器残留
    avcodec_flush_buffers(aucodec);      // 清音频解码器残留
    videoQueue->clear();
    decodeOneVideoFrame();
}

void DecoderWorker::decodeBatch()
{
    int videoFrames = 0;
    const int MAX_PER_BATCH = 3;
    int ret = 0;

    while (videoFrames < MAX_PER_BATCH) {
        ret = av_read_frame(Ctx, pkt);
        if (ret < 0) break;           // EOF 或错误

        if (pkt->stream_index == AVSTREAM) {
            avcodec_send_packet(avcodec, pkt);
            av_packet_unref(pkt);
            while (avcodec_receive_frame(avcodec, frame) == 0) {
                if (!swsnew) {
                    sws = sws_getContext(
                        frame->width, frame->height, AV_PIX_FMT_YUV420P,
                        frame->width, frame->height, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, nullptr, nullptr, nullptr);
                    swsnew = true;
                }
                int rgbStride = frame->width * 3;
                int rgbSize   = frame->height * rgbStride;
                uint8_t *rgbBuf = (uint8_t *)malloc(rgbSize);
                sws_scale(sws, frame->data, frame->linesize, 0, frame->height,
                          &rgbBuf, &rgbStride);

                QImage decoded(rgbBuf, frame->width, frame->height,
                               QImage::Format_RGB888);
                FrameData data;
                data.pts_us = frame->pts * av_q2d(videoTimeBase) * 1000000;
                data.image = decoded.copy();
                videoQueue->push(data);
                free(rgbBuf);
                videoFrames++;
                break;
            }
        }
        else if (pkt->stream_index == AUSTREAM) {
            avcodec_send_packet(aucodec, pkt);
            av_packet_unref(pkt);

            while (avcodec_receive_frame(aucodec, frame) == 0) {
                if (!swrnew) {
                    swr = swr_alloc();
                    av_opt_set_int(swr, "in_sample_fmt",  frame->format, 0);
                    av_opt_set_int(swr, "in_sample_rate", frame->sample_rate, 0);
                    av_opt_set_chlayout(swr, "in_chlayout", &frame->ch_layout, 0);
                    av_opt_set_int(swr, "out_sample_fmt",  AV_SAMPLE_FMT_S16, 0);
                    av_opt_set_int(swr, "out_sample_rate", m_audioSampleRate / speed, 0);
                    av_opt_set_chlayout(swr, "out_chlayout", &frame->ch_layout, 0);
                    swr_init(swr);
                    swrnew = true;
                }

                int outSamplesMax = (int)(frame->nb_samples / speed) + 256;
                int outBytes = outSamplesMax * frame->ch_layout.nb_channels * 2;
                std::vector<uint8_t> outBuf(outBytes);
                uint8_t *outPtr = outBuf.data();

                int outSamples = swr_convert(swr,
                                             &outPtr, frame->nb_samples,
                                             (const uint8_t **)frame->data, frame->nb_samples);
                totalWritten += outSamples;
                audioClock = totalWritten / ((double)m_audioSampleRate / speed) * 1000000;

                emit audioReady(QByteArray((const char*)outBuf.data(), outSamples
                                           * frame->ch_layout.nb_channels * 2),
                                aucodec->ch_layout.nb_channels, m_audioSampleRate);
            }
        }
        else {
            av_packet_unref(pkt);      // 字幕/数据流，释放
        }
    }

    if (ret < 0) {
        m_timer->stop();
        videoQueue->setDone();
        emit closeFrame();
    }
}

void DecoderWorker::start()
{
    m_timer->start();
}
void DecoderWorker::decodeOneVideoFrame()
{
    av_packet_unref(pkt);
    while(av_read_frame(Ctx,pkt)>=0)
    {
        if(pkt->stream_index==AVSTREAM)
        {
            avcodec_send_packet(avcodec,pkt);
            av_packet_unref(pkt);
            while(avcodec_receive_frame(avcodec,frame)==0)
            {
                if (!swsnew) {
                    sws = sws_getContext(
                        frame->width, frame->height, AV_PIX_FMT_YUV420P,
                        // AV_PIX_FMT_YUV420P: H.264 最常见像素格式
                        frame->width, frame->height, AV_PIX_FMT_RGB24,
                        // AV_PIX_FMT_RGB24: QImage::Format_RGB888 对应的格式
                        SWS_BILINEAR, nullptr, nullptr, nullptr);
                    swsnew = true;
                }
                int rgbStride = frame->width * 3;    // RGB24 每行字节数
                int rgbSize   = frame->height * rgbStride;
                uint8_t *rgbBuf = (uint8_t *)malloc(rgbSize);
                sws_scale(sws, frame->data, frame->linesize, 0, frame->height,
                          &rgbBuf, &rgbStride);
                // sws_scale: YUV420P → RGB24，输出到 rgbBuf

                QImage decoded(rgbBuf, frame->width, frame->height,
                               QImage::Format_RGB888);
                FrameData data;
                data.pts_us=frame->pts * av_q2d(videoTimeBase) * 1000000;
                data.image=decoded.copy();
                videoQueue->push(data);
                free(rgbBuf);
                audioClock = data.pts_us;
                totalWritten = audioClock * (m_audioSampleRate/speed) / 1000000;
                break;
            }
            break;
        }
        else{
            av_packet_unref(pkt);
        }
    }
}

void DecoderWorker::setSpeed(double seep)
{
    speed=seep;
    swrnew =false;
    m_timer->setInterval((int)(m_baseFrameDelay / speed));
    double posSec=audioClock/1000000.0;
    totalWritten=posSec*(m_audioSampleRate/speed);
}