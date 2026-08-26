#include "audioformat.h"
#include <qdebug.h>

AudioFormat::AudioFormat(qint64 sample, int ch_layout, QObject *parent)
    : QObject{parent}
{
    fmt.setSampleRate(sample);
    fmt.setChannelCount(ch_layout);
    fmt.setSampleFormat(QAudioFormat::Int16);

    mic=QMediaDevices::defaultAudioInput();

}

void AudioFormat::start()
{
    if(src){
        src.reset();
    }
    src=std::make_unique<QAudioSource>(mic,fmt,this);
    io=src->start();
    if(!io){qDebug()<<"麦克风打开失败";return;}
}

void AudioFormat::stop()
{
    if(src){
        src->stop();
        src.reset();
    }
    io=nullptr;
}
