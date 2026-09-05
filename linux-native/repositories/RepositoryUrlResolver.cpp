#include "RepositoryUrlResolver.h"

#include <QRegularExpression>
#include <QUrl>

namespace CloudStream {

namespace {
QString ensureHttps(QString value) {
    value = value.trimmed();
    if (!value.startsWith("http://", Qt::CaseInsensitive) &&
        !value.startsWith("https://", Qt::CaseInsensitive)) {
        value.prepend("https://");
    }
    return value;
}
}

QString RepositoryUrlResolver::resolveShortForm(QString value) {
    value = value.trimmed();
    if (value.startsWith("cloudstreamrepo://", Qt::CaseInsensitive)) {
        return ensureHttps(value.mid(QString("cloudstreamrepo://").size()));
    }
    if (value.startsWith("https://cs.repo", Qt::CaseInsensitive)) {
        const auto question = value.indexOf('?');
        if (question >= 0) return ensureHttps(value.mid(question + 1));
        auto payload = value.mid(QString("https://cs.repo").size());
        while (payload.startsWith('/')) payload.remove(0, 1);
        return ensureHttps(payload);
    }
    if (value.startsWith("cs.repo/", Qt::CaseInsensitive)) {
        return ensureHttps(value.mid(QString("cs.repo/").size()));
    }
    if (value.startsWith('!')) {
        return "https://py.md/" + value.mid(1);
    }
    static const QRegularExpression shortCode("^[A-Za-z0-9_-]+$");
    if (shortCode.match(value).hasMatch()) {
        return "https://cutt.ly/" + value;
    }
    return value;
}

QString RepositoryUrlResolver::manifestUrl(QString value) {
    value = resolveShortForm(value);
    const QUrl parsed(value);
    if (parsed.host().compare("github.com", Qt::CaseInsensitive) == 0) {
        const auto parts = parsed.path().split('/', Qt::SkipEmptyParts);
        if (parts.size() == 2) {
            return "https://raw.githubusercontent.com/" + parts[0] + "/" +
                   parts[1] + "/master/repo.json";
        }
    }
    return value;
}

bool RepositoryUrlResolver::isSupportedInput(const QString &value) {
    const auto resolved = resolveShortForm(value);
    const QUrl parsed(resolved);
    return parsed.isValid() &&
           (parsed.scheme().compare("https", Qt::CaseInsensitive) == 0 ||
            parsed.scheme().compare("http", Qt::CaseInsensitive) == 0) &&
           !parsed.host().isEmpty();
}

} // namespace CloudStream
