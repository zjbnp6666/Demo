#include "framequeue.h"

void FrameQueue::push(const FrameData &frame)
{
    QMutexLocker locker(&mutex);
    while (queue.size() >= MAX_SIZE)
        notFull.wait(&mutex);

    queue.append(frame);
    notEmpty.wakeOne();
}

void FrameQueue::push(FrameData &&frame)
{
    QMutexLocker locker(&mutex);
    while(queue.size()>=MAX_SIZE)
    {
        notFull.wait(&mutex);
    }
    queue.append(std::move(frame));
    notEmpty.wakeOne();
}

FrameData FrameQueue::pop()
{
    QMutexLocker locker(&mutex);
    while (queue.isEmpty() && !m_done)
        notEmpty.wait(&mutex);

    if (queue.isEmpty())
        return FrameData();          // EOF，返回空帧

    FrameData data = queue.takeFirst();
    notFull.wakeOne();
    return data;
}

void FrameQueue::clear()
{
    QMutexLocker locker(&mutex);
    queue.clear();
    m_done = false;
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
    m_done = true;
    notEmpty.wakeAll();              // 唤醒 pop()，让其返回空帧
}
