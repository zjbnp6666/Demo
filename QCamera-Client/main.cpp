#include "logger.h"
#include "widget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Widget w;
    static Logger &logger=Logger::instance();
    logger.info("此程序启动");
    w.show();
    return QApplication::exec();
}
