#ifndef AUDIOSINKE_H
#define AUDIOSINKE_H

#include <QObject>
#include<QAudioSink>
#include<QAudioFormat>
#include<QIODevice>
#include"logger.h"
class AudioSinke : public QObject
{
    Q_OBJECT
public:
    explicit AudioSinke(QObject *parent = nullptr);
    void strat();
    void stop();
    void addPcm(const char * pcm,int len);
    qint64 processedUSecs() const;//声卡从ring buffer取走、送去播放的时长(微秒)=播放头;不是写入量
    int pendingBytes() const;//缓冲区还剩多少没播(缓冲区容量-还剩多少空位)
    std::unique_ptr<QAudioSink> sink;
    void setVolume(qreal vol);
    int byteFree()const;//返回缓冲区剩余区域(缓冲区空着的位置)
    qint64 bytesPerSec();//返回每秒的字节数不写死数字44100;
private:
    QAudioFormat fmt;
    QIODevice *io=nullptr;
    qint64 addPcmsum=0;
signals:
};

#endif // AUDIOSINKE_H
