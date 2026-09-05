#pragma once

#include <QString>

namespace CloudStream {

class RepositoryUrlResolver final {
public:
    static QString resolveShortForm(QString value);
    static QString manifestUrl(QString value);
    static bool isSupportedInput(const QString &value);
};

} // namespace CloudStream
