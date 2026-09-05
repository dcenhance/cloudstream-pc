#include "PlayerCommand.h"

#include <QFileInfo>
#include <QUrl>

namespace CloudStream {

bool PlayerCommand::isValidTarget(const QString &target) {
    if (target.trimmed().isEmpty()) return false;
    const QFileInfo file(target);
    if (file.exists() && file.isFile()) return true;
    const QUrl url(target);
    return url.isValid() &&
           (url.scheme().compare("http", Qt::CaseInsensitive) == 0 ||
            url.scheme().compare("https", Qt::CaseInsensitive) == 0 ||
            url.scheme().compare("rtmp", Qt::CaseInsensitive) == 0 ||
            url.scheme().compare("rtsp", Qt::CaseInsensitive) == 0 ||
            url.scheme().compare("magnet", Qt::CaseInsensitive) == 0);
}

QStringList PlayerCommand::arguments(const QString &target,
                                     const QString &ipcSocket,
                                     const QString &watchLaterDirectory,
                                     const QString &subtitleFile,
                                     double resumePositionSeconds) {
    QStringList result{
        "--force-window=yes",
        "--input-ipc-server=" + ipcSocket,
        "--save-position-on-quit=yes",
        "--resume-playback=yes",
        "--watch-later-directory=" + watchLaterDirectory,
        "--keep-open=no",
    };
    if (!subtitleFile.isEmpty()) result << "--sub-file=" + subtitleFile;
    if (resumePositionSeconds > 0.0) {
        result << "--start=" + QString::number(resumePositionSeconds, 'g', 15);
    }
    result << target;
    return result;
}

} // namespace CloudStream
