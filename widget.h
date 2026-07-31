#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QThread>
#include <QMessageBox>
#include "framequeue.h"
#include "decoderworker.h"
#include<QTimer>
#include<QDebug>
#include<QImage>
#include<QKeyEvent>
#include<QPainter>
#include "qaudio.h"
#include<QSlider>
#include<QLabel>
#include<QFileDialog>
#include<QMouseEvent>
#include<QDateTime>
#include<QStandardPaths>
#include<QListWidget>
#include<QPushButton>
#include<QDragEnterEvent>
#include<QDropEvent>
#include<QMimeData>
#include<QUrl>
#include<QFileInfo>
QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

private:
    Ui::Widget *ui;

    // ========== 线程 / 解码 ==========
    FrameQueue *videoQueue = nullptr;
    DecoderWorker *worker = nullptr;
    QThread *decoderThread = nullptr;

    // ========== 音视频同步 ==========
    QImage pendingFrame;                  // 太早到达暂存等显示的帧
    long long pendingPts = -1;            // 缓存帧的 PTS，-1 = 无缓存

    // ========== 播放控制 ==========
    QTimer *times = nullptr;              // 定时器，按帧间隔触发 strat()
    bool paused = false;                  // 暂停标记
    void strat();                         // 每帧回调：取帧→音视频同步→显示

    // ========== 进度条 / Seek ==========
    double   totalDurationSec = 0;        // 视频总时长（秒）
    QSlider *timesize = nullptr;          // 进度条
    QLabel  *zero = nullptr;              // 左侧时间标签 "当前时间"
    QLabel  *stop = nullptr;              // 右侧时间标签 "总时长"
    bool     m_isSeeking = false;         // 用户正在拖进度条，strat 暂停更新滑块
    void     seek(int ret);                      // 重播版
    void     seek(); //跳转
    bool justSeeket=false;

    // ========== 音量控制 ==========
    QSlider *volSlider = nullptr;         // 垂直音量滑条
    QLabel  *volLabel  = nullptr;         // 音量图标标签

    // ========== 倍速 ==========
    QLabel *speedLabel = nullptr;         // 倍速标签（1.0x）
    double spedd=1.0;

    // ========== 播放列表 ==========
    QListWidget *playlist = nullptr;      // 播放列表（右侧面板）
    QPushButton *playlistBtn = nullptr;   // 切换按钮
    QPushButton *addFilesBtn = nullptr;   // 添加文件按钮
    bool playlistVisible = false;
    void addToPlaylist(const QStringList &files);

    // ========== 画面 / 输入 ==========
    QImage image;                         // 当前显示的画面帧
    AudioPlayer *audio = nullptr;         // QAudioSink 封装，write PCM 即可播放
    bool newaudio=false;
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;

    //========窗口缩放=========//
    void resizeEvent(QResizeEvent *) override;

    //========开始双击全屏=========//
    void mouseDoubleClickEvent(QMouseEvent *)override;

    //双击确认退出
    void closeEvent(QCloseEvent *event)override;

    //========拖拽文件=========//
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onAudioReady(QByteArray pcm, int channels, int sampleRate);
    void onDurationReady(double seconds);
    void onOpenFailed(const QString &msg);
    void closeOver();
signals:
    void requestSeek(int64_t SeekTarget);
    void speedvalue(double seepvalue);
    void openstart(const QString filePath);
};
#endif // WIDGET_H
