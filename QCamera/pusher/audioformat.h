#ifndef AUDIOFORMAT_H
#define AUDIOFORMAT_H

#include <QObject>
#include<QMediaDevices>
#include<QAudioFormat>
#include<QAudioSource>
class AudioFormat : public QObject
{
    Q_OBJECT
public:
    explicit AudioFormat(qint64 sample,int ch_layout,QObject *parent = nullptr);
    void start();
    void stop();
    QIODevice *io=nullptr;
private:
    QAudioDevice mic;
    QAudioFormat fmt;
    std::unique_ptr<QAudioSource> src;
signals:
};

#endif // AUDIOFORMAT_H
