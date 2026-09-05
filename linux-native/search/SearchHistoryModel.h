#pragma once

#include <QStringList>

namespace CloudStream {

class SearchHistoryModel final {
public:
    static QStringList add(const QStringList &existing, const QString &query, int limit = 12);
    static QStringList remove(const QStringList &existing, const QString &query);
};

} // namespace CloudStream
