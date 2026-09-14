#ifndef TQUEUE_H
#define TQUEUE_H

#include <QMutex>
#include <QWaitCondition>
#include <QList>
#include <QImage>
#include <atomic>

struct FrameData {
    QImage   image;
    long long pts_us = 0;  // 微秒
};
struct AudioData{
    QByteArray data;
    long long pts_us=0;
};

class FrameQueue
{
public:
    void push(const FrameData &frame);
    void push(FrameData &&frame);       // 移动版本，避免拷贝 QImage
    FrameData pop();
    void clear();
    bool isEmpty();
    void setDone();        // EOF 标记，唤醒 pop()

    void push1(const AudioData &frame);
    void push1(AudioData &&frame);       // 移动版本，避免拷贝 QImage
    AudioData pop1();
    void clear1();
    bool isEmpty1();
    void setDone1();        // EOF 标记，唤醒 pop()

    int takeDropFullVideo();   // 取走并清零 视频队列满丢弃 计数
    int takeDropFullAudio();   // 取走并清零 音频队列满丢弃 计数

private:
    QMutex mutex;
    QWaitCondition notEmpty;
    QWaitCondition notFull;
    QList<FrameData> queue;
    static const int MAX_SIZE = 5;
    bool m_done = false;
    std::atomic<int> m_dropFullVideo{0};
    std::atomic<int> m_dropFullAudio{0};

    QMutex amutex;
    QWaitCondition anotEmpty;
    QWaitCondition anotFull;
    QList<AudioData> queue1;
    bool m_adone = false;
};

#endif // TQUEUE_H
