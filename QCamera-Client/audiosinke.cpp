#include "audiosinke.h"

AudioSinke::AudioSinke(QObject *parent)
    : QObject{parent}
{
    fmt.setSampleRate(44100);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);
}

void AudioSinke::strat()
{
    if(sink)
    {
        sink->stop();
        sink.reset();
        io.reset();
    }
    sink=std::make_unique<QAudioSink>(fmt,this);
    sink->setBufferSize(44100*2*2/5);
    QIODevice *ii=sink->start();
    io.reset(ii);
}

void AudioSinke::stop()
{
    if(sink)
    {
        sink->stop();
        sink.reset();
    }
    io.reset();
}

void AudioSinke::addPcm(const char* pcm, int len)
{
    if(io)
        io->write(pcm,len);
}

qint64 AudioSinke::processedUSecs() const
{
    return sink? sink->processedUSecs():0;
}

void AudioSinke::setVolume(qreal vol)
{
    if(sink) sink->setVolume(vol);
}

int AudioSinke::byteFree()const
{
    return sink?sink->bytesFree():0;
}
