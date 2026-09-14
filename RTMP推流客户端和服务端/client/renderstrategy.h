#ifndef RENDERSTRATEGY_H
#define RENDERSTRATEGY_H
#include<QRect>
#include<QSize>
class Renderstrategy
{
public:
    virtual ~Renderstrategy()=default;
    virtual QRect targetRect(const QSize &frame,const QSize &view) const =0;
};
// 等比缩放 + 黑边（现在的样子）
class KeepAspectStrategy : public Renderstrategy {
public:
    QRect targetRect(const QSize &frame, const QSize &view) const override {
        QSize s = frame;
        s.scale(view, Qt::KeepAspectRatio);
        return QRect((view.width()  - s.width())  / 2,
                     (view.height() - s.height()) / 2,
                     s.width(), s.height());
    }
};

// 拉伸填满（变形）
class StretchStrategy : public Renderstrategy {
public:
    QRect targetRect(const QSize &, const QSize &view) const override {
        return QRect(0, 0, view.width(), view.height());
    }
};

// 等比裁剪填满（无黑边，四周裁掉）
class CropStrategy : public Renderstrategy {
public:
    QRect targetRect(const QSize &frame, const QSize &view) const override {
        QSize s = frame;
        s.scale(view, Qt::KeepAspectRatioByExpanding);
        return QRect((view.width()  - s.width())  / 2,
                     (view.height() - s.height()) / 2,
                     s.width(), s.height());
    }
};
#endif // RENDERSTRATEGY_H
