#ifndef MY_AUDIOPLAYER_H
#define MY_AUDIOPLAYER_H

#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QElapsedTimer>

// 包装 QAudioSink：创建→启动→write PCM 即可
class AudioPlayer : public QObject {
    Q_OBJECT
public:
    AudioPlayer(int sampleRate, int channels, QObject *parent = nullptr);
    ~AudioPlayer();
    void start();
    void stop();
    void addPCM(const char *data, int len);
    QAudioSink *m_sink = nullptr;  //QAudioSink 播放器本体
    void setVolume(qreal vol);
private:
    QAudioFormat m_fmt; //告诉播放器 我要播放什么格式
    QIODevice *m_dev = nullptr;  // start() 返回的可写管道
    QElapsedTimer m_timer;       // 从 start() 开始计时
    long long m_bytesWritten = 0; // 累计写入字节数
};

#endif // MY_AUDIOPLAYER_H
