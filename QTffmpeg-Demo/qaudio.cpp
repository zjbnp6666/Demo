#include "qaudio.h"

AudioPlayer::AudioPlayer(int sampleRate, int channels, QObject *parent)
    : QObject(parent)
{
    m_format.setSampleRate(sampleRate);
    m_format.setChannelCount(channels);
    m_format.setSampleFormat(QAudioFormat::Int16);
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

void AudioPlayer::start()
{
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
    }
    m_sink = new QAudioSink(m_format);
    m_sink->setBufferSize(m_format.bytesForDuration(80000));
    m_device = m_sink->start();
    m_timer.start();
    m_bytesWritten = 0;
}

void AudioPlayer::stop()
{
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
        m_sink = nullptr;
    }
    m_device = nullptr;
}

void AudioPlayer::addPCM(const char *data, int len)
{
    if (m_device)
        m_device->write(data, len);
}

void AudioPlayer::setVolume(qreal vol)
{
    if (m_sink)
        m_sink->setVolume(vol);
}
