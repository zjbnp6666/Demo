#include "widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    // ==================== 文件选择对话框 ====================
    QFileDialog dlg(this, "选择视频", "", "视频文件(*.mp4)");
    dlg.setOption(QFileDialog::DontUseNativeDialog);
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setStyleSheet(R"(
        QFileDialog { background-color: #0a0a0f; }
        QLabel { color: #6c7086; font-size: 11px; }
        QLineEdit { background: #1e1e2e; color: #cccccc; border: 1px solid #333; border-radius: 3px; padding: 3px; }
        QPushButton { background: #1e1e2e; color: #cccccc; border: 1px solid #333; border-radius: 3px; padding: 4px 12px; }
        QPushButton:hover { background: #00d4ff; color: #0a0a0f; }
        QListView { background: #0a0a0f; color: #cccccc; border: none; outline: none; }
        QListView::item { padding: 5px 8px; }
        QListView::item:hover { background: #1e1e2e; }
        QListView::item:selected { background: #00d4ff; color: #0a0a0f; }
    )");
    if (dlg.exec() != QDialog::Accepted) return;
    QString filepath = dlg.selectedFiles().constFirst();
    setFocusPolicy(Qt::StrongFocus);

    // ==================== 解码线程初始化 ====================
    m_videoQueue = new FrameQueue;
    m_worker = new DecoderWorker(m_videoQueue);
    m_decoderThread = new QThread(this);
    m_worker->moveToThread(m_decoderThread);

    connect(m_decoderThread, &QThread::started,
            m_worker, [=]() { m_worker->open(filepath); });
    connect(m_worker, &DecoderWorker::audioReady,
            this, &Widget::onAudioReady);
    connect(m_worker, &DecoderWorker::durationReady,
            this, &Widget::onDurationReady);
    connect(m_worker, &DecoderWorker::openFailed,
            this, &Widget::onOpenFailed);
    connect(m_decoderThread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    // ==================== UI 控件 ====================
    m_playbackTimer = new QTimer(this);

    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_seekSlider->setValue(0);
    m_seekSlider->setRange(0, 100);

    m_currentTimeLbl = new QLabel("0:00", this);
    m_durationLbl    = new QLabel("0:00", this);

    m_volumeSlider = new QSlider(Qt::Vertical, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeLabel = new QLabel("Vol", this);

    m_speedLabel = new QLabel("1.0x", this);
    m_speedLabel->setStyleSheet(R"(
        QLabel {
            color: #00d4ff; font-size: 13px; font-weight: bold;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(0,212,255,0.15), stop:1 rgba(123,47,247,0.15));
            border: 1px solid #00d4ff; border-radius: 4px; padding: 3px 10px;
        }
    )");
    m_speedLabel->adjustSize();

    m_playlistBtn = new QPushButton("≡ 列表", this);
    m_playlistBtn->setStyleSheet(R"(
        QPushButton {
            color: #00d4ff; font-size: 12px; font-weight: bold;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(0,212,255,0.15), stop:1 rgba(123,47,247,0.15));
            border: 1px solid #00d4ff; border-radius: 4px; padding: 3px 10px;
        }
        QPushButton:hover { background: rgba(0,212,255,0.3); border-color: #ffffff; color: #ffffff; }
    )");
    m_playlistBtn->adjustSize();

    m_addFilesBtn = new QPushButton("+", this);
    m_addFilesBtn->setStyleSheet(R"(
        QPushButton {
            color: #00d4ff; font-size: 14px; font-weight: bold;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(0,212,255,0.15), stop:1 rgba(123,47,247,0.15));
            border: 1px solid #00d4ff; border-radius: 4px; padding: 3px 8px;
        }
        QPushButton:hover { background: rgba(0,212,255,0.3); border-color: #ffffff; color: #ffffff; }
    )");
    m_addFilesBtn->adjustSize();

    m_playlistWidget = new QListWidget(this);
    m_playlistWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_playlistWidget->setDefaultDropAction(Qt::MoveAction);
    m_playlistWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_playlistWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_playlistWidget->hide();

    setAcceptDrops(true);

    // ==================== 全局 QSS ====================
    setStyleSheet(R"(
        Widget { background-color: #0a0a0f; }

        QSlider::groove:horizontal {
            height: 3px; background: #1e1e2e; border-radius: 1px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #00d4ff, stop:1 #7b2ff7); border-radius: 1px;
        }
        QSlider::handle:horizontal {
            width: 14px; height: 14px; margin: -6px 0;
            background: #ffffff; border: 2px solid #00d4ff; border-radius: 7px;
        }
        QSlider::handle:horizontal:hover { background: #00d4ff; border-color: #ffffff; }

        QSlider::groove:vertical {
            width: 3px; background: #1e1e2e; border-radius: 1px;
        }
        QSlider::sub-page:vertical {
            background: qlineargradient(x1:0, y1:1, x2:0, y2:0,
                stop:0 #00d4ff, stop:1 #7b2ff7); border-radius: 1px;
        }
        QSlider::handle:vertical {
            width: 14px; height: 14px; margin: 0 -6px;
            background: #ffffff; border: 2px solid #00d4ff; border-radius: 7px;
        }
        QSlider::handle:vertical:hover { background: #00d4ff; border-color: #ffffff; }

        QLabel {
            color: #6c7086; font-size: 11px;
            font-family: "Consolas", "Courier New", monospace; background: transparent;
        }

        QListWidget {
            background: rgba(10,10,30,0.95); border: 1px solid rgba(255,255,255,0.1);
            border-radius: 8px; outline: none;
        }
        QListWidget::item { color: #999; padding: 8px 12px; border-bottom: 1px solid rgba(255,255,255,0.05); }
        QListWidget::item:hover { background: rgba(255,255,255,0.06); color: #ddd; }
        QListWidget::item:selected { background: rgba(0,212,255,0.15); color: #00d4ff; border-left: 2px solid #00d4ff; }
    )");

    addToPlaylist(dlg.selectedFiles());
    resize(800, 600);

    // ==================== 信号连接 ====================
    // 定时器 → 每帧处理
    connect(m_playbackTimer, &QTimer::timeout, this, &Widget::onPlaybackTick);

    // 进度条拖动 → 更新时间标签
    connect(m_seekSlider, &QSlider::valueChanged, this, [=](int val) {
        int m = val / 60;
        int s = val % 60;
        m_currentTimeLbl->setText(QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0')));
    });

    // 进度条按下/松开 → seek
    connect(m_seekSlider, &QAbstractSlider::sliderPressed,
            this, [=]() { m_isSeeking = true; });
    connect(m_seekSlider, &QAbstractSlider::sliderReleased,
            this, static_cast<void(Widget::*)()>(&Widget::seek));

    // 音量
    connect(m_volumeSlider, &QSlider::valueChanged, this, [=](int val) {
        if (m_audioPlayer)
            m_audioPlayer->setVolume(val / 100.0);
    });

    // 解码线程信号
    connect(m_worker, &DecoderWorker::isSeekFalse,
            this, [=]() { m_isSeeking = false; });
    connect(m_worker, &DecoderWorker::closeFrame,
            this, &Widget::onPlaybackFinished);

    // 跨线程信号
    connect(this, &Widget::requestSeek,  m_worker, &DecoderWorker::seek);
    connect(this, &Widget::speedChanged, m_worker, &DecoderWorker::setSpeed);

    // 播放列表按钮
    connect(m_playlistBtn, &QPushButton::clicked, this, [=]() {
        m_playlistVisible = !m_playlistVisible;
        m_playlistWidget->setVisible(m_playlistVisible);
        m_addFilesBtn->setVisible(m_playlistVisible);
    });

    // 添加文件
    connect(m_addFilesBtn, &QPushButton::clicked, [=]() {
        QStringList files = QFileDialog::getOpenFileNames(
            this, "添加视频", "", "视频文件(*.mp4 *.mkv *.avi *.mov *.flv)");
        if (!files.isEmpty()) addToPlaylist(files);
    });

    // 双击播放列表项 → 切换视频
    connect(m_playlistWidget, &QListWidget::itemDoubleClicked,
            this, [=](QListWidgetItem *item) {
        if (!item) return;
        QString path = item->data(Qt::UserRole).toString();

        m_playbackTimer->stop();
        m_playbackSpeed = 1.0;
        m_speedLabel->setText("1.0x");
        emit speedChanged(m_playbackSpeed);

        if (m_audioPlayer) {
            delete m_audioPlayer;
            m_audioPlayer = nullptr;
        }
        m_audioNeedsInit = false;
        m_pendingFrame = QImage();
        m_pendingPts   = -1;
        m_videoQueue->clear();
        emit fileOpened(path);
        m_currentImage = QImage();
        m_justSeeked   = false;
        m_paused       = false;
    });
    connect(this, &Widget::fileOpened, m_worker, &DecoderWorker::open);

    QWidget::resizeEvent(nullptr);
    m_decoderThread->start();
}

Widget::~Widget()
{
    m_playbackTimer->stop();
    if (m_decoderThread) {
        QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
        m_decoderThread->quit();
        m_decoderThread->wait();
    }
    if (m_audioPlayer) {
        m_audioPlayer->stop();
        delete m_audioPlayer;
    }
    delete m_videoQueue;
}

// ==================== 每帧回调 ====================
void Widget::onPlaybackTick()
{
    if (m_isSeeking) return;
    if (m_paused)    return;

    // 更新进度条（音频时钟驱动）
    m_seekSlider->setValue((int)(m_worker->audioClock / 1000000));

    // 检查缓存帧是否到显示时间
    if (!m_pendingFrame.isNull() && m_pendingPts <= m_worker->audioClock + 30000) {
        m_currentImage = m_pendingFrame;
        m_pendingFrame = QImage();
        update();
    }

    FrameData data = m_videoQueue->pop();   // 阻塞等帧

    if (m_justSeeked) {
        m_justSeeked = false;               // seek 后首帧不检查"太晚"
    } else if (data.pts_us < m_worker->audioClock - 100000) {
        return;                             // 太晚，丢弃
    }

    if (data.pts_us > m_worker->audioClock + 30000) {
        m_pendingFrame = data.image;        // 太早，缓存
        m_pendingPts   = data.pts_us;
        return;
    }

    m_currentImage = data.image;
    m_pendingFrame = QImage();
    update();
}

// ==================== 渲染 ====================
void Widget::paintEvent(QPaintEvent *)
{
    if (m_currentImage.isNull()) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    int margin = 35;
    QRect target = rect().adjusted(0, 0, 0, -margin);
    painter.drawImage(target, m_currentImage);
}

// ==================== 键盘事件 ====================
void Widget::keyPressEvent(QKeyEvent *event)
{
    // 空格 → 暂停/播放
    if (event->key() == Qt::Key_Space) {
        m_paused = !m_paused;
    }

    // Esc → 退出全屏
    if (event->key() == Qt::Key_Escape && isFullScreen()) {
        showNormal();
        m_seekSlider->show();   m_currentTimeLbl->show(); m_durationLbl->show();
        m_volumeSlider->show(); m_volumeLabel->show();    m_speedLabel->show();
        m_playlistBtn->show();
    }

    // S → 截图到桌面
    if (event->key() == Qt::Key_S && !m_currentImage.isNull()) {
        QString path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
                     + "/" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")
                     + "_截图.png";
        m_currentImage.save(path);
    }

    // 倍速快捷键（互斥）
    if (event->key() == Qt::Key_1) {
        m_playbackSpeed = 1.0;
    } else if (event->key() == Qt::Key_2) {
        m_playbackSpeed = 1.5;
    } else if (event->key() == Qt::Key_3) {
        m_playbackSpeed = 2.0;
    }

    // 左箭头按住 → 临时 1.5x
    if (event->key() == Qt::Key_Left && !event->isAutoRepeat()) {
        m_playbackSpeed = 1.5;
    }

    // 统一处理倍速变更
    if (event->key() == Qt::Key_1 || event->key() == Qt::Key_2
        || event->key() == Qt::Key_3
        || (event->key() == Qt::Key_Left && !event->isAutoRepeat())) {
        m_speedLabel->setText(QString("%1x").arg(m_playbackSpeed, 0, 'f', 1));
        emit speedChanged(m_playbackSpeed);
        m_audioNeedsInit = false;
        m_playbackTimer->setInterval((int)(16 / m_playbackSpeed));
    }
}

void Widget::keyReleaseEvent(QKeyEvent *event)
{
    // 左箭头松开 → 恢复 1.0x
    if (event->key() == Qt::Key_Left && !event->isAutoRepeat()) {
        m_playbackSpeed = 1.0;
        m_speedLabel->setText("1.0x");
        emit speedChanged(m_playbackSpeed);
        m_audioNeedsInit = false;
        m_playbackTimer->setInterval((int)(16 / m_playbackSpeed));
    }
}

// ==================== 窗口事件 ====================
void Widget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    int barY = height() - 35;

    m_seekSlider->setGeometry(50, barY, width() - 100, 20);
    m_currentTimeLbl->move(10, barY + 2);
    m_durationLbl->move(width() - 45, barY + 2);

    int volH = 100;
    m_volumeSlider->setGeometry(width() - 30, barY - volH, 20, volH);
    m_volumeLabel->move(width() - 28, barY - volH - 18);

    m_speedLabel->move(10, barY - 26);
    m_playlistBtn->move(65, barY - 26);
    m_addFilesBtn->move(130, barY - 26);

    int plW = 280;
    m_playlistWidget->setGeometry(width() - plW, 0, plW, barY - 4);
}

void Widget::mouseDoubleClickEvent(QMouseEvent *)
{
    if (isFullScreen()) {
        showNormal();
        m_seekSlider->show();   m_currentTimeLbl->show(); m_durationLbl->show();
        m_volumeSlider->show(); m_volumeLabel->show();
        m_speedLabel->show();   m_playlistBtn->show();
    } else {
        showFullScreen();
        m_seekSlider->hide();   m_currentTimeLbl->hide(); m_durationLbl->hide();
        m_volumeSlider->hide(); m_volumeLabel->hide();
        m_speedLabel->hide();   m_playlistBtn->hide();
    }
}

void Widget::closeEvent(QCloseEvent *event)
{
    m_paused = true;
    m_videoQueue->clear();
    int ret = QMessageBox::question(this, "确认", "确定要退出吗");
    if (ret == QMessageBox::Yes) {
        event->accept();
    } else {
        m_paused = false;
        event->ignore();
    }
}

// ==================== 拖拽文件 ====================
void Widget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void Widget::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            files.append(url.toLocalFile());
    }
    if (!files.isEmpty())
        addToPlaylist(files);
}

void Widget::addToPlaylist(const QStringList &files)
{
    for (const QString &f : files) {
        QFileInfo fi(f);
        bool dup = false;
        for (int i = 0; i < m_playlistWidget->count(); i++) {
            if (m_playlistWidget->item(i)->data(Qt::UserRole).toString() == f) {
                dup = true; break;
            }
        }
        if (dup) continue;

        QListWidgetItem *item = new QListWidgetItem(fi.fileName());
        item->setData(Qt::UserRole, f);
        item->setToolTip(f);
        m_playlistWidget->addItem(item);
    }
}

// ==================== 解码线程回调 ====================
void Widget::onAudioReady(QByteArray pcm, int channels, int sampleRate)
{
    if (!m_audioNeedsInit) {
        if (m_audioPlayer) {
            delete m_audioPlayer;
            m_audioPlayer = nullptr;
        }
        m_audioPlayer = new AudioPlayer(sampleRate, channels);
        m_audioPlayer->start();
        m_audioNeedsInit = true;
    }
    m_audioPlayer->addPCM(pcm.constData(), pcm.size());
}

void Widget::onDurationReady(double seconds)
{
    m_seekSlider->setRange(0, (int)seconds);
    m_seekSlider->setValue(0);
    int min = (int)seconds / 60;
    int sec = (int)seconds % 60;
    m_durationLbl->setText(QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0')));
    m_playbackTimer->setInterval((int)(16 / m_playbackSpeed));
    m_playbackTimer->start();
}

void Widget::onOpenFailed(const QString &msg)
{
    QMessageBox::critical(this, "错误", msg, QMessageBox::Cancel);
    qApp->exit(1);
}

void Widget::onPlaybackFinished()
{
    m_playbackTimer->stop();
    int ret = QMessageBox::information(this, "提示",
        "视频已播放完毕，是否重播？", QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_playbackTimer->start();
        seekTo(0);
    } else {
        hide();
        deleteLater();
    }
}

// ==================== Seek ====================
void Widget::seekTo(int64_t targetUs)
{
    if (m_audioPlayer) {
        m_audioPlayer->stop();
        m_audioPlayer->start();
    }
    m_pendingFrame = QImage();
    m_pendingPts   = -1;
    m_worker->pendingSeek = true;
    m_videoQueue->clear();
    emit requestSeek(targetUs);
    m_justSeeked = true;
}

void Widget::seek()
{
    seekTo((int64_t)m_seekSlider->value() * AV_TIME_BASE);
}
