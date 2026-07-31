#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H
#include<QThread>
#include<QWaitCondition>
#include<QMutex>
#include<QList>
#include <qimage.h>
struct FrameData{
    QImage image;
    long long pts_us;
};

class FrameQueue
{
    QMutex mutex;
    QWaitCondition notEmpty;//空了让出队线程休眠
    QWaitCondition notFull;//满了就让入队线程休眠
    QList<FrameData> queue;
    static const int MAX_SIZE=5;
    bool m_done=false;
public:
    void push(const FrameData &frame);
    FrameData pop();
    void clear();
    bool isEmpty();
    void setDone();
    void setGO();
};

#endif // FRAMEQUEUE_H
