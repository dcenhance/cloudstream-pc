#include "ProviderSelectionModel.h"
#include "ProviderPreferenceFilter.h"

#include <QMap>
#include <QSet>

namespace CloudStream {

QString ProviderSelectionModel::key(const QJsonObject &provider) {
    return provider.value("jarPath").toString() + "\n" + provider.value("name").toString();
}

QString ProviderSelectionModel::languageFlag(const QString &language) {
    static const QMap<QString, QString> flags{
        {"de", "🇩🇪"}, {"en", "🇬🇧"}, {"it", "🇮🇹"}, {"es", "🇪🇸"},
        {"fr", "🇫🇷"}, {"vi", "🇻🇳"}, {"hi", "🇮🇳"}, {"ja", "🇯🇵"},
        {"ko", "🇰🇷"}, {"zh", "🇨🇳"}, {"pt", "🇵🇹"}, {"ar", "🌐"},
    };
    const auto code = language.trimmed().toLower();
    return flags.value(code, code.isEmpty() ? QString("🌐") : code.left(2).toUpper());
}

QString ProviderSelectionModel::displayName(const QJsonObject &provider) {
    const auto name = provider.value("name").toString();
    const auto flag = languageFlag(provider.value("language").toString());
    return flag.isEmpty() ? name : flag + " " + name;
}

bool ProviderSelectionModel::shouldReloadHome(const QString &currentProviderKey,
                                              const QJsonObject &selectedProvider) {
    return currentProviderKey.isEmpty() || key(selectedProvider) != currentProviderKey;
}

QList<QJsonObject> ProviderSelectionModel::homeCandidates(const QList<QJsonObject> &providers) {
    QList<QJsonObject> candidates;
    for (const auto &provider : providers) {
        if (provider.value("hasMainPage").toBool()) candidates << provider;
    }
    return candidates;
}

QList<QJsonObject> ProviderSelectionModel::selectableHomeCandidates(
        const QList<QJsonObject> &providers, const QString &language, bool allowNsfw) {
    return homeCandidates(ProviderPreferenceFilter::apply(providers, language, allowNsfw));
}

QList<QJsonObject> ProviderSelectionModel::automaticHomeCandidates(
        const QList<QJsonObject> &providers, const QString &language, bool allowNsfw) {
    return homeCandidates(ProviderPreferenceFilter::apply(providers, language, allowNsfw));
}

QList<QJsonObject> ProviderSelectionModel::searchCandidates(const QList<QJsonObject> &providers,
                                                            const QStringList &selectedKeys) {
    if (selectedKeys.isEmpty()) return providers;
    const QSet<QString> wanted(selectedKeys.begin(), selectedKeys.end());
    QList<QJsonObject> candidates;
    for (const auto &provider : providers) {
        if (wanted.contains(key(provider))) candidates << provider;
    }
    return candidates;
}

QList<QJsonObject> ProviderSelectionModel::effectiveSearchCandidates(
        const QList<QJsonObject> &providers, const QStringList &selectedKeys,
        const QString &language, bool allowNsfw) {
    const auto allowed = ProviderPreferenceFilter::apply(providers, language, allowNsfw);
    return searchCandidates(allowed, selectedKeys);
}

QList<QJsonObject> ProviderSelectionModel::mergeValidatedProviders(
        const QList<QJsonObject> &existing, const QString &extensionName,
        const QString &artifactPath, const QJsonArray &providers) {
    QList<QJsonObject> merged;
    QSet<QString> keys;
    for (const auto &provider : existing) {
        if (provider.value("extensionName").toString() == extensionName) continue;
        const auto providerKey = key(provider);
        if (keys.contains(providerKey)) continue;
        keys.insert(providerKey);
        merged << provider;
    }
    for (const auto &value : providers) {
        auto provider = value.toObject();
        if (provider.value("name").toString().trimmed().isEmpty()) continue;
        provider.insert("mode", "provider");
        provider.insert("jarPath", artifactPath);
        provider.insert("extensionName", extensionName);
        const auto providerKey = key(provider);
        if (keys.contains(providerKey)) continue;
        keys.insert(providerKey);
        merged << provider;
    }
    return merged;
}

} // namespace CloudStream
