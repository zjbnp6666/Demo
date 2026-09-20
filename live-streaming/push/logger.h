#ifndef LOGGER_H
#define LOGGER_H

#include<QString>
#include<QFile>
#include<QTimer>
#include<QDateTime>
#include<QDebug>
#include<QMutex>
class Logger
{
public:
    static Logger& instance();
    void debug(const QString& msg);
    void info(const QString& msg);
    void warn(const QString& msg);
    void error(const QString& msg);
    void setLevel(int lv);
private:
    Logger();
    QFile m_file;
    void write(const QString& level,const QString& msg);  // 真正的写
    int TEMP=0;
    QMutex mutex;
};

#endif // LOGGER_H
