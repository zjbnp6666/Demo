#include "audioformat.h"
#include <qdebug.h>

AudioFormat::AudioFormat(qint64 sample, int ch_layout, QObject *parent)
    : QObject{parent}
{
    fmt.setSampleRate(sample);
    fmt.setChannelCount(ch_layout);
    fmt.setSampleFormat(QAudioFormat::Int16);

    mic=QMediaDevices::defaultAudioInput();//使用默认设备 之前只在
    //构造函数调用一次 导致设备更换 QAudioSource收不到信息

}

void AudioFormat::start()
{
    mic=QMediaDevices::defaultAudioInput();//重新更新默认设备 选中为当前设备 防止使用之前设备
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
