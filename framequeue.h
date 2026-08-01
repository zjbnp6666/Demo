#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H

#include <QMutex>
#include <QWaitCondition>
#include <QList>
#include <QImage>

struct FrameData {
    QImage   image;
    long long pts_us = 0;  // 微秒
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

private:
    QMutex mutex;
    QWaitCondition notEmpty;
    QWaitCondition notFull;
    QList<FrameData> queue;
    static const int MAX_SIZE = 5;
    bool m_done = false;
};

#endif // FRAMEQUEUE_H
