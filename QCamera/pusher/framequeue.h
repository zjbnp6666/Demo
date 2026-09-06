#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H
#include<QMutexLocker>
#include <qvideoframe.h>
#include<QWaitCondition>
#include<vector>
#include<QList>
struct Nv12Frame {                    // 主线程往 worker 传的原始帧
    std::vector<uint8_t> y, uv;        // NV12 两平面
    int width=0, height=0, strideY=0, strideUV=0;
    qint64 ts_ms=0;
};
//不能直接传QVideoFrame 他是引用计数句柄 多人共享数据的 并没有复制一份 那块内存不属于你 属于相机 如果下一帧来了 可能这块内存
//就变成别人的了
class FrameQueue
{
public:
    FrameQueue();
    QList<Nv12Frame> queue;
    bool isEmpty();
    void push(Nv12Frame &&frame);
    Nv12Frame pop();
    void stop();
    QMutex mutex;
    QWaitCondition notEmpty;//有数据
    const int Maxsize=5;
    bool stopInit=false;
};

#endif // FRAMEQUEUE_H
