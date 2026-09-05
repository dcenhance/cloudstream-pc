#include "SearchHistoryModel.h"

namespace CloudStream {

QStringList SearchHistoryModel::remove(const QStringList &existing, const QString &query) {
    const auto normalized = query.simplified();
    QStringList result;
    for (const auto &value : existing) {
        if (value.simplified().compare(normalized, Qt::CaseInsensitive) != 0) result << value.simplified();
    }
    return result;
}

QStringList SearchHistoryModel::add(const QStringList &existing, const QString &query, int limit) {
    const auto normalized = query.simplified();
    if (normalized.isEmpty() || limit <= 0) return existing;
    auto result = remove(existing, normalized);
    result.prepend(normalized);
    while (result.size() > limit) result.removeLast();
    return result;
}

} // namespace CloudStream
