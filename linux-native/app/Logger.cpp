#include "Logger.h"

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QTextStream>

namespace CloudStream {
namespace {
QString logPath;
QMutex logMutex;

const char *levelName(QtMsgType type) {
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARN";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "UNKNOWN";
}

void fileMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &message) {
    QMutexLocker lock(&logMutex);
    QFile file(logPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
               << " [" << levelName(type) << "] " << message << '\n';
    }
    if (type == QtFatalMsg) abort();
}
}

void installFileLogging(const QString &path) {
    logPath = path;
    qInstallMessageHandler(fileMessageHandler);
}

} // namespace CloudStream
