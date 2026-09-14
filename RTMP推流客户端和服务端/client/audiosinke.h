#ifndef AUDIOSINKE_H
#define AUDIOSINKE_H

#include <QObject>
#include<QAudioSink>
#include<QAudioFormat>
#include<QIODevice>
class AudioSinke : public QObject
{
    Q_OBJECT
public:
    explicit AudioSinke(QObject *parent = nullptr);
    void strat();
    void stop();
    void addPcm(const char * pcm,int len);
    qint64 processedUSecs() const;//暴露声卡播放时长
    std::unique_ptr<QAudioSink> sink;
    void setVolume(qreal vol);
    int byteFree()const;//返回缓冲区剩余区域
private:
    QAudioFormat fmt;
    std::unique_ptr<QIODevice> io;
signals:
};

#endif // AUDIOSINKE_H
