#include "ExtensionListFilter.h"

namespace CloudStream {

bool ExtensionListFilter::matches(const QString &searchableText, const QString &query) {
    const auto tokens = query.simplified().split(' ', Qt::SkipEmptyParts);
    for (const auto &token : tokens) {
        if (!searchableText.contains(token, Qt::CaseInsensitive)) return false;
    }
    return true;
}

} // namespace CloudStream
