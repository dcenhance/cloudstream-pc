#pragma once

#include <QString>
#include <QStringList>

namespace CloudStream {

class PlayerCommand final {
public:
    static bool isValidTarget(const QString &target);
    static QStringList arguments(const QString &target,
                                 const QString &ipcSocket,
                                 const QString &watchLaterDirectory,
                                 const QString &subtitleFile = {},
                                 double resumePositionSeconds = 0.0);
};

} // namespace CloudStream
