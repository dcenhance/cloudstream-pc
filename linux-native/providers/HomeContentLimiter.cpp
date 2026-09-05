#include "HomeContentLimiter.h"

#include <QJsonObject>
#include <algorithm>

namespace CloudStream {

QJsonArray HomeContentLimiter::limit(const QJsonArray &sections, int maximumSections, int maximumItemsPerSection) {
    return limitRange(sections, 0, maximumSections, maximumItemsPerSection);
}

QJsonArray HomeContentLimiter::limitRange(const QJsonArray &sections, int firstSection,
                                          int maximumSections, int maximumItemsPerSection) {
    QJsonArray limited;
    const auto first = std::clamp(firstSection, 0, int(sections.size()));
    const auto sectionCount = std::min(int(sections.size()) - first,
                                       std::max(0, maximumSections));
    for (int offset = 0; offset < sectionCount; ++offset) {
        const auto sectionIndex = first + offset;
        auto section = sections[sectionIndex].toObject();
        const auto items = section.value("items").toArray();
        section.insert("totalItems", items.size());
        QJsonArray visibleItems;
        const auto itemCount = std::min<qsizetype>(items.size(), std::max(0, maximumItemsPerSection));
        for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex) visibleItems.append(items[itemIndex]);
        section.insert("items", visibleItems);
        limited.append(section);
    }
    return limited;
}

bool HomeContentLimiter::equivalent(const QJsonArray &left, const QJsonArray &right) {
    return left == right;
}

int HomeContentLimiter::nextSectionCount(int currentCount, int totalCount, int batchSize) {
    const auto total = std::max(0, totalCount);
    const auto current = std::clamp(currentCount, 0, total);
    return std::min(total, current + std::max(0, batchSize));
}

} // namespace CloudStream
