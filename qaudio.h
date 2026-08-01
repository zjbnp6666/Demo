#ifndef MY_AUDIOPLAYER_H
#define MY_AUDIOPLAYER_H

#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QElapsedTimer>

class AudioPlayer : public QObject {
    Q_OBJECT
public:
    AudioPlayer(int sampleRate, int channels, QObject *parent = nullptr);
    ~AudioPlayer();

    void start();
    void stop();
    void addPCM(const char *data, int len);
    void setVolume(qreal vol);

private:
    QAudioFormat  m_format;
    QAudioSink   *m_sink = nullptr;
    QIODevice    *m_device = nullptr;
    QElapsedTimer m_timer;
    long long     m_bytesWritten = 0;
};

#endif // MY_AUDIOPLAYER_H
