#pragma once
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QStringList>

namespace CloudStream {
// A Search session owns each provider's next page and deduplicates returned titles.
class SearchPaginationModel {
public:
    void reset(const QStringList &providers) {
        nextPages_.clear();
        seen_.clear();
        for (const auto &provider : providers) nextPages_.insert(provider, 1);
    }

    int pageFor(const QString &provider) const { return nextPages_.value(provider, 0); }
    int count() const { return seen_.size(); }

    QList<QJsonObject> complete(const QString &provider, int page, bool hasNext,
                                const QList<QJsonObject> &results) {
        if (page < 1 || pageFor(provider) != page) return {};
        nextPages_[provider] = hasNext ? page + 1 : 0;
        QList<QJsonObject> unique;
        for (const auto &result : results) {
            const auto identity = result.value("_jarPath").toString() + "\n" +
                result.value("apiName").toString(result.value("_providerName").toString()) + "\n" +
                result.value("url").toString();
            if (seen_.contains(identity)) continue;
            seen_.insert(identity);
            unique.append(result);
        }
        return unique;
    }

private:
    QHash<QString, int> nextPages_;
    QSet<QString> seen_;
};
} // namespace CloudStream
