#include "qaudio.h"

AudioPlayer::AudioPlayer(int sampleRate, int channels, QObject *parent)
    : QObject(parent)
{
    //sanoleRate表示这堆数据的采样率
    //QAudioSink内部逻辑时
    //输出时长=样本数/sampleRate
    m_fmt.setSampleRate(sampleRate);
    m_fmt.setChannelCount(channels);
    m_fmt.setSampleFormat(QAudioFormat::Int16);
}

AudioPlayer::~AudioPlayer() { stop(); }

void AudioPlayer::start() {
    m_sink = new QAudioSink(m_fmt, this);
    m_sink->setBufferSize(m_fmt.bytesForDuration(80000));  // 80ms 缓冲
    m_dev = m_sink->start();  // 返回可写的 QIODevice，Qt 内部开始播放
    m_timer.start();
    m_bytesWritten = 0;
}

void AudioPlayer::stop() {
    if (m_sink) { m_sink->stop(); m_sink = nullptr; }
    m_dev = nullptr;
}

void AudioPlayer::addPCM(const char *data, int len) {
    if (m_dev) {
        m_dev->write(data, len);
        m_bytesWritten += len;
    }
}
void AudioPlayer::setVolume(qreal vol)
{
    if(m_sink)
        m_sink->setVolume(vol);
}
