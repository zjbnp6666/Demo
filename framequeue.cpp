#include "framequeue.h"
void FrameQueue::push(const FrameData &frame)
{
    QMutexLocker locker(&mutex);
    while(queue.size()>=MAX_SIZE)
    {
        notFull.wait(&mutex);
    }
    queue.append(frame);
    notEmpty.wakeOne();
}

FrameData FrameQueue::pop()
{
    QMutexLocker locker(&mutex);
    while(queue.isEmpty()&&!m_done)
    {
        notEmpty.wait(&mutex);
    }
    if(queue.isEmpty())
    {
        return FrameData();
    }
    notFull.wakeOne();
    return queue.takeFirst();
}

void FrameQueue::clear()
{
    QMutexLocker locker(&mutex);
    queue.clear();
    notFull.wakeAll();
    notEmpty.wakeAll();
}

bool FrameQueue::isEmpty()
{
    QMutexLocker locker(&mutex);
    return queue.isEmpty();
}

void FrameQueue::setDone()
{
    QMutexLocker locker(&mutex);
    m_done=true;
    notEmpty.wakeAll();
}

void FrameQueue::setGO()
{
    QMutexLocker locker(&mutex);
    m_done=false;
}
