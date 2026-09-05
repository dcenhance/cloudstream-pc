#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace CloudStream {

class ProviderSelectionModel final {
public:
    static QString key(const QJsonObject &provider);
    static QString languageFlag(const QString &language);
    static QString displayName(const QJsonObject &provider);
    static bool shouldReloadHome(const QString &currentProviderKey,
                                 const QJsonObject &selectedProvider);
    static QList<QJsonObject> homeCandidates(const QList<QJsonObject> &providers);
    static QList<QJsonObject> selectableHomeCandidates(const QList<QJsonObject> &providers,
                                                        const QString &language,
                                                        bool allowNsfw);
    static QList<QJsonObject> automaticHomeCandidates(const QList<QJsonObject> &providers,
                                                       const QString &language,
                                                       bool allowNsfw);
    static QList<QJsonObject> searchCandidates(const QList<QJsonObject> &providers,
                                               const QStringList &selectedKeys);
    static QList<QJsonObject> effectiveSearchCandidates(const QList<QJsonObject> &providers,
                                                        const QStringList &selectedKeys,
                                                        const QString &language,
                                                        bool allowNsfw);
    static QList<QJsonObject> mergeValidatedProviders(const QList<QJsonObject> &existing,
                                                      const QString &extensionName,
                                                      const QString &artifactPath,
                                                      const QJsonArray &providers);
};

} // namespace CloudStream
