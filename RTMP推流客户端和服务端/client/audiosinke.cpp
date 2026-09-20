#include "audiosinke.h"
#include <qdebug.h>

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
        io=nullptr;
        sink.reset();

    }
    sink=std::make_unique<QAudioSink>(fmt,this);
    sink->setBufferSize(44100*2*2/5);
    io=sink->start();
    //qDebug() << "[strat] io=" << (io!=nullptr)
      //       << " byteFree=" << (sink ? sink->bytesFree() : -1);
}

void AudioSinke::stop()
{
    if(sink)
    {
        sink->stop();
        sink.reset();
    }
    io=nullptr;
}

void AudioSinke::addPcm(const char* pcm, int len)
{
    if(io){
        //addPcmsum+=len;
        //qDebug()<<"PCM总数:"<<addPcmsum;
        io->write(pcm,len);
    }
}

qint64 AudioSinke::processedUSecs() const
{
    return sink? sink->processedUSecs():0;
}

int AudioSinke::pendingBytes() const
{
    return sink?sink->bufferSize()-sink->bytesFree():0;
}

void AudioSinke::setVolume(qreal vol)
{
    if(sink) sink->setVolume(vol);
}

int AudioSinke::byteFree()const
{
    return sink?sink->bytesFree():0;
}

qint64 AudioSinke::bytesPerSec()
{
    return fmt.sampleRate()*fmt.channelCount()*(fmt.sampleFormat()==QAudioFormat::Int16?2:4);
}
