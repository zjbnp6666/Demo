#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include<QImage>
#include<QFont>
#include<QTimer>
#include"avdeio.h"
#include<QPainter>
#include<QThread>
#include"audiosinke.h"
#include"videowidget.h"
#include<QLineEdit>
#include<QPushButton>
#include<QVBoxLayout>
#include<QHBoxLayout>
#include<QGridLayout>
#include<QToolButton>
#include<QMessageBox>
#include<QDialog>
#include<QKeyEvent>
#include<QMouseEvent>
#include<QRadioButton>
#include<QButtonGroup>
#include<QLabel>
#include <QProgressBar>
#include<QSlider>
#include<QDebug>
#include<QElapsedTimer>
#include <QMediaDevices>
QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

private:
    Ui::Widget *ui;
    std::unique_ptr<QTimer> time;
    std::unique_ptr<VideoWidget> videoWidget;
    std::unique_ptr<Avdeio> deio;
    void start(QImage image);//传画面接口
    void streamstart();//30ms时钟播放 以及音视频同步API
    void setupUI();//画UI控件
    void setStystDialog();//设置连接
    void setStop();//暂停功能

    void keyPressEvent(QKeyEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void toggleFullScreen();//设置全屏按钮
    void setRenderMode(int id);//切换模式槽函数
    void switchStream(const QString &newUrl);//切换清晰度重连
    void connectDeio();//重建 deio 后重连信号
    void relatchAudioClock();//重新锁存时钟

    std::unique_ptr<AudioSinke> au;
    std::unique_ptr<QTimer> utime;
    bool headpst=false;//音频首包

    bool images=false;//是否缓存
    FrameData q=FrameData();

    bool pause=false;//暂停

    long long audioClock=0;//音频主时钟
    qint64 procBace=0; //第一帧音频时 声卡已播放基准
    long long clockBasePts=0;//第一帧的音频pts

    QString url="http://127.0.0.1:8000/live/stream_720p.flv";//URL连接地址(默认高清,HTTP-FLV)

    //暂停和等待动画
    std::unique_ptr<QProgressBar> loadingBar;
    void updateLoading();

    //UI成员
    std::unique_ptr<QLineEdit> Urlname;//设置URL地址按钮
    std::unique_ptr<QPushButton> btnconnect;//连接按钮
    std::unique_ptr<QToolButton> btnSyst;//系统设置按钮
    std::unique_ptr<QToolButton> btnPause;//暂停按钮
    std::unique_ptr<QButtonGroup> rdoGroup;//更换画面模式按钮组
    QSlider *volSlider = nullptr;//音量进度条
    QToolButton *btnMute = nullptr;//静音按钮
    bool muted = false;
    //清晰度切换按钮
    std::unique_ptr<QButtonGroup> rdoQuality;
    std::unique_ptr<QToolButton> btnHD;
    std::unique_ptr<QToolButton> btnSD;
    std::unique_ptr<QToolButton> btnLD;
    QDialog dialogSyst;//设置弹窗

    QElapsedTimer t;
    bool tstart=false;

    QElapsedTimer headWait;//音频就绪时钟 等久了不要了

    qint64 bytesOerSec;//每秒字节数=return byresOerSec
};
#endif // WIDGET_H
