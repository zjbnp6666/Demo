
#include "tqueue.h"
void FrameQueue::push(const FrameData &frame)
{
    QMutexLocker locker(&mutex);
    while (queue.size() >= MAX_SIZE)
        queue.takeFirst();          // 满了丢老帧，给新帧腾位
    queue.append(frame);
    notEmpty.wakeOne();
}

void FrameQueue::push(FrameData &&frame)
{
    QMutexLocker locker(&mutex);
    while(queue.size()>=MAX_SIZE)
        queue.takeFirst();          // 同上
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
    return data;
}

void FrameQueue::clear()
{
    QMutexLocker locker(&mutex);
    queue.clear();
    m_done = false;
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
void FrameQueue::push1(const AudioData &frame)
{
    QMutexLocker locker(&amutex);
    while (queue1.size() >= 10)
        queue1.takeFirst();          // 满了丢老块，给新块腾位
    queue1.append(frame);
    anotEmpty.wakeOne();
}

void FrameQueue::push1(AudioData &&frame)
{
    QMutexLocker locker(&amutex);
    while(queue1.size()>=10)
        queue1.takeFirst();          // 同上
    queue1.append(std::move(frame));
    anotEmpty.wakeOne();
}

AudioData FrameQueue::pop1()
{
    QMutexLocker locker(&amutex);
    while (queue1.isEmpty() && !m_adone)
        anotEmpty.wait(&amutex);

    if (queue1.isEmpty())
        return AudioData();          // EOF，返回空帧

    AudioData data = queue1.takeFirst();
    return data;
}

void FrameQueue::clear1()
{
    QMutexLocker locker(&amutex);
    queue1.clear();
    m_adone = false;
    anotFull.wakeAll();
    anotEmpty.wakeAll();
}

bool FrameQueue::isEmpty1()
{
    QMutexLocker locker(&amutex);
    return queue1.isEmpty();
}

void FrameQueue::setDone1()
{
    QMutexLocker locker(&amutex);
    m_adone = true;
    anotEmpty.wakeAll();              // 唤醒 pop()，让其返回空帧
}

int FrameQueue::depth1()
{
    QMutexLocker l(&amutex);
    return queue1.size();
}

int FrameQueue::depth()
{
    QMutexLocker l(&mutex);  return queue.size();
}