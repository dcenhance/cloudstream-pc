#pragma once

#include <QJsonArray>

namespace CloudStream {

class HomeContentLimiter final {
public:
    static QJsonArray limit(const QJsonArray &sections, int maximumSections, int maximumItemsPerSection);
    static QJsonArray limitRange(const QJsonArray &sections, int firstSection,
                                 int maximumSections, int maximumItemsPerSection);
    static bool equivalent(const QJsonArray &left, const QJsonArray &right);
    static int nextSectionCount(int currentCount, int totalCount, int batchSize);
};

} // namespace CloudStream
