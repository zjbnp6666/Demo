#include "logger.h"

Logger::Logger() {
    m_file.setFileName("app.log");
    m_file.open(QIODevice::Append|QIODevice::Text);
}
Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::debug(const QString &msg)
{
    if(TEMP<=0)
        write("DEBUG",msg);
}

void Logger::info(const QString &msg)
{
    if(TEMP<=1)
        write("INFO",msg);
}

void Logger::warn(const QString &msg)
{
    if(TEMP<=2)
        write("WARN",msg);
}

void Logger::error(const QString &msg)
{
    if(TEMP<=3)
        write("ERROR",msg);
}

void Logger::setLevel(int lv)
{
    TEMP=lv;
}

void Logger::write(const QString &level, const QString &msg)
{
    QMutexLocker locket(&mutex);
    QString text = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
                   + " [" + level + "] " + msg + "\n";
    m_file.write(text.toUtf8());
    qDebug()<<text;
    m_file.flush();
}



