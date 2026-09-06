#include "framequeue.h"

FrameQueue::FrameQueue() {}




void FrameQueue::push(Nv12Frame &&frame)
{
    QMutexLocker locket(&mutex);
    while (queue.size()>=Maxsize) {
        queue.pop_front();//满了丢老帧换新帧 不存在满了的情况
    }
    queue.push_back(std::move(frame));
    notEmpty.wakeOne();
}

Nv12Frame FrameQueue::pop()
{
    QMutexLocker locket(&mutex);
    while(isEmpty()&&!stopInit){
        notEmpty.wait(&mutex);
    }
    while(isEmpty()){
        return Nv12Frame();
    }
    Nv12Frame f=std::move(queue.back());
    queue.pop_back();
    return f;
}

void FrameQueue::stop()
{
    QMutexLocker locket(&mutex);
    stopInit = true;
    notEmpty.wakeAll();
}

bool FrameQueue::isEmpty()
{
    if(queue.empty()){
        return true;
    }
    else{
        return false;
    }
}

