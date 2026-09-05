#pragma once

#include <QString>
#include <QStringList>

namespace CloudStream {

class ExtensionListFilter final {
public:
    static bool matches(const QString &searchableText, const QString &query);
};

} // namespace CloudStream
