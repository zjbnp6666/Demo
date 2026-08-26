#include "decoderworker.h"

DecoderWorker::DecoderWorker(FrameQueue *vq, QObject *parent)
    : QObject(parent), m_videoQueue(vq)
{
    m_packet = av_packet_alloc();
    m_frame  = av_frame_alloc();
    m_timer  = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &DecoderWorker::decodeBatch);
}

DecoderWorker::~DecoderWorker()
{
    m_timer->stop();
    av_packet_free(&m_packet);
    av_frame_free(&m_frame);
    m_videoCodecCtx.reset();
    m_audioCodecCtx.reset();
    swr_free(&m_swr);
    sws_freeContext(m_sws);
    m_formatCtx.reset();
}

// ==================== 打开文件 ====================
void DecoderWorker::open(const QString &filepath)
{
    // 重开时先清理旧的
    if (m_formatCtx) {
        m_formatCtx.reset();
        m_videoCodecCtx.reset();
        m_audioCodecCtx.reset();
        m_audioCodecCtx = nullptr;
        m_videoCodecCtx = nullptr;

        m_videoQueue->clear();
        swr_free(&m_swr);
        sws_freeContext(m_sws);
        m_sws = nullptr;
        m_audioSamplesWritten = 0;
        audioClock = 0;
        m_videoStreamIndex = -1;
        m_audioStreamIndex = -1;
        m_speed = 1.0;
    }

    QByteArray rawPath = filepath.toLocal8Bit();
    const char *cPath  = rawPath.constData();

    AVFormatContext *raw=nullptr;
    if (avformat_open_input(&raw, cPath, nullptr, nullptr)) {
        emit openFailed("文件打开失败");
        return;
    }
    m_formatCtx.reset(raw);
    avformat_find_stream_info(m_formatCtx.get(), nullptr);

    m_totalDuration = (double)m_formatCtx->duration / AV_TIME_BASE;
    emit durationReady(m_totalDuration);

    double frameDelay = 0;
    for (unsigned i = 0; i < m_formatCtx->nb_streams; i++) {
        AVStream *stream = m_formatCtx->streams[i];
        AVCodecParameters *codecpar = stream->codecpar;

        if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_audioStreamIndex = i;
            const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
            AVCodecContext *raw=nullptr;
            raw = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(raw, codecpar);
            avcodec_open2(raw, codec, nullptr);
            m_videoCodecCtx.reset(raw);
            m_audioSampleRate = m_audioCodecCtx->sample_rate;
        }

        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
            AVCodecContext *raw=nullptr;
            raw = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(raw, codecpar);
            avcodec_open2(raw, codec, nullptr);
            m_videoCodecCtx.reset(raw);

            frameDelay = (double)stream->avg_frame_rate.den
                       / stream->avg_frame_rate.num * 1000.0;
            m_videoTimeBase  = stream->time_base;
            m_frameDelayMs   = frameDelay;
        }
    }

    m_timer->setInterval((int)(frameDelay / m_speed));
    m_timer->setSingleShot(false);
    m_timer->start();
    m_swsReady = false;
    m_swrReady = false;
}

// ==================== 停止 / 变速 ====================
void DecoderWorker::stop()
{
    m_timer->stop();
}

void DecoderWorker::setSpeed(double speed)
{
    m_speed = speed;
    m_swrReady = false;
    m_timer->setInterval((int)(m_frameDelayMs / m_speed));
    double posSec = audioClock / 1000000.0;
    m_audioSamplesWritten = (long long)(posSec * (m_audioSampleRate / m_speed));
}

// ==================== Seek ====================
void DecoderWorker::seek(int64_t targetUs)
{
    m_timer->stop();

    av_seek_frame(m_formatCtx.get(), -1, targetUs, AVSEEK_FLAG_BACKWARD);
    m_videoCodecCtx.reset();
    m_audioCodecCtx.reset();
    m_videoQueue->clear();
    decodeOneVideoFrame();

    pendingSeek = false;
    m_timer->start();
    emit isSeekFalse();
}

// ==================== 定时解码（每 16ms） ====================
void DecoderWorker::decodeBatch()
{
    if (pendingSeek) return;

    int videoFrames = 0;
    const int MAX_PER_BATCH = 3;
    int ret = 0;

    while (videoFrames < MAX_PER_BATCH) {
        if (pendingSeek) return;

        ret = av_read_frame(m_formatCtx.get(), m_packet);
        if (ret < 0) break;

        // ---- 视频包 ----
        if (m_packet->stream_index == m_videoStreamIndex) {
            avcodec_send_packet(m_videoCodecCtx.get(), m_packet);
            av_packet_unref(m_packet);

            while (avcodec_receive_frame(m_videoCodecCtx.get(), m_frame) == 0) {
                if (!m_swsReady) {
                    m_sws = sws_getContext(
                        m_frame->width, m_frame->height, AV_PIX_FMT_YUV420P,
                        m_frame->width, m_frame->height, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, nullptr, nullptr, nullptr);
                    m_swsReady = true;
                }

                int rgbStride = m_frame->width * 3;
                int rgbSize   = m_frame->height * rgbStride;
                uint8_t *rgbBuf = (uint8_t *)malloc(rgbSize);
                sws_scale(m_sws, m_frame->data, m_frame->linesize,
                          0, m_frame->height, &rgbBuf, &rgbStride);

                QImage decoded(rgbBuf, m_frame->width, m_frame->height,
                               QImage::Format_RGB888);

                FrameData data;
                data.pts_us = m_frame->pts * av_q2d(m_videoTimeBase) * 1000000.0;
                data.image  = decoded.copy();
                m_videoQueue->push(std::move(data));
                free(rgbBuf);
                videoFrames++;
                break;
            }
        }
        // ---- 音频包 ----
        else if (m_packet->stream_index == m_audioStreamIndex) {
            avcodec_send_packet(m_audioCodecCtx.get(), m_packet);
            av_packet_unref(m_packet);

            while (avcodec_receive_frame(m_audioCodecCtx.get(), m_frame) == 0) {
                if (!m_swrReady) {
                    m_swr = swr_alloc();
                    av_opt_set_int(m_swr, "in_sample_fmt",  m_frame->format, 0);
                    av_opt_set_int(m_swr, "in_sample_rate", m_frame->sample_rate, 0);
                    av_opt_set_chlayout(m_swr, "in_chlayout", &m_frame->ch_layout, 0);
                    av_opt_set_int(m_swr, "out_sample_fmt",  AV_SAMPLE_FMT_S16, 0);
                    av_opt_set_int(m_swr, "out_sample_rate",
                                   m_audioSampleRate / m_speed, 0);
                    av_opt_set_chlayout(m_swr, "out_chlayout", &m_frame->ch_layout, 0);
                    swr_init(m_swr);
                    m_swrReady = true;
                }

                int outSamplesMax = (int)(m_frame->nb_samples / m_speed) + 256;
                int outBytes = outSamplesMax * m_frame->ch_layout.nb_channels * 2;
                std::vector<uint8_t> outBuf(outBytes);
                uint8_t *outPtr = outBuf.data();

                int outSamples = swr_convert(m_swr,
                                             &outPtr, m_frame->nb_samples,
                                             (const uint8_t **)m_frame->data,
                                             m_frame->nb_samples);
                m_audioSamplesWritten += outSamples;
                audioClock = (long long)(m_audioSamplesWritten
                              / ((double)m_audioSampleRate / m_speed) * 1000000.0);

                emit audioReady(
                    QByteArray((const char *)outBuf.data(),
                               outSamples * m_frame->ch_layout.nb_channels * 2),
                    m_audioCodecCtx->ch_layout.nb_channels,
                    m_audioSampleRate);
            }
        }
        // ---- 字幕/数据流 ----
        else {
            av_packet_unref(m_packet);
        }
    }

    if (ret < 0) {
        m_timer->stop();
        m_videoQueue->setDone();
        emit closeFrame();
    }
}

// ==================== seek 后解码一帧视频 ====================
void DecoderWorker::decodeOneVideoFrame()
{
    av_packet_unref(m_packet);
    while (av_read_frame(m_formatCtx.get(), m_packet) >= 0) {
        if (m_packet->stream_index == m_videoStreamIndex) {
            avcodec_send_packet(m_videoCodecCtx.get(), m_packet);
            av_packet_unref(m_packet);

            if (avcodec_receive_frame(m_videoCodecCtx.get(), m_frame) == 0) {
                if (!m_swsReady) {
                    m_sws = sws_getContext(
                        m_frame->width, m_frame->height, AV_PIX_FMT_YUV420P,
                        m_frame->width, m_frame->height, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, nullptr, nullptr, nullptr);
                    m_swsReady = true;
                }

                int rgbStride = m_frame->width * 3;
                int rgbSize   = m_frame->height * rgbStride;
                uint8_t *rgbBuf = (uint8_t *)malloc(rgbSize);
                sws_scale(m_sws, m_frame->data, m_frame->linesize,
                          0, m_frame->height, &rgbBuf, &rgbStride);

                QImage decoded(rgbBuf, m_frame->width, m_frame->height,
                               QImage::Format_RGB888);

                FrameData data;
                data.pts_us = m_frame->pts * av_q2d(m_videoTimeBase) * 1000000.0;
                data.image  = decoded.copy();
                m_videoQueue->push(std::move(data));
                free(rgbBuf);

                audioClock = data.pts_us;
                m_audioSamplesWritten = (long long)(audioClock
                    * (m_audioSampleRate / m_speed) / 1000000.0);
                return;
            }
        } else {
            av_packet_unref(m_packet);
        }
    }
}
