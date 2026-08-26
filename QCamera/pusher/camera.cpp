#include "camera.h"

Camera::Camera(QObject *parent)
    : QObject{parent}
{
    QCamera *cameras = new QCamera(this);
    QMediaCaptureSession *sessions = new QMediaCaptureSession(this);
    QVideoSink *sinks = new QVideoSink(this);
    this->camera.reset(cameras);
    this->sink.reset(sinks);
    this->session.reset(sessions);

    session.get()->setCamera(camera.get());
    session.get()->setVideoSink(sink.get());
    camera.get()->start();
}
