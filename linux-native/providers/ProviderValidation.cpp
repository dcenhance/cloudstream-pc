#include "ProviderValidation.h"

#include <QStringList>

namespace CloudStream {

ProviderValidationResult ProviderValidation::classify(int exitCode, int providerCount, const QString &errorText) {
    if (exitCode == 0 && providerCount > 0) return ProviderValidationResult::Runnable;
    const auto lower = errorText.toLower();
    if (lower.contains("android/webkit/") || lower.contains("android.webkit.") ||
        lower.contains("android.graphics.drawable")) {
        return ProviderValidationResult::AndroidOnly;
    }
    const QStringList integrationMarkers{
        "ui.home.homeviewmodel", "ui.syncwatchtype", "syncproviders.syncapi",
        "ui.player.csplayerevent", "android.content.contentresolver",
    };
    for (const auto &marker : integrationMarkers) {
        if (lower.contains(marker)) return ProviderValidationResult::UtilityOnly;
    }
    if (lower.contains("registered no providers through load()") && lower.contains("nosuchmethodexception")) {
        return ProviderValidationResult::RequiresConfiguration;
    }
    if (lower.contains("no baseplugin or direct mainapi implementation found") ||
        lower.contains("no baseplugin implementation found") ||
        lower.contains("registered no providers through load()")) {
        return ProviderValidationResult::UtilityOnly;
    }
    return ProviderValidationResult::Failed;
}

} // namespace CloudStream
