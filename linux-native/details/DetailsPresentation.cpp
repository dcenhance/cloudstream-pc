#include "DetailsPresentation.h"

namespace CloudStream {

QString DetailsPresentation::backdropUrl(const QJsonObject &details) {
    const auto backdrop = details.value("backgroundPosterUrl").toString().trimmed();
    return backdrop.isEmpty() ? details.value("posterUrl").toString().trimmed() : backdrop;
}

QStringList DetailsPresentation::facts(const QJsonObject &details, const QString &provider) {
    QStringList values;
    const auto year = details.value("year").toInt();
    if (year > 0) values << QString::number(year);
    const auto type = details.value("type").toString().trimmed();
    if (!type.isEmpty()) values << type;
    const auto duration = details.value("duration").toInt();
    if (duration > 0) values << QString::number(duration) + " min";
    const auto contentRating = details.value("contentRating").toString().trimmed();
    if (!contentRating.isEmpty()) values << contentRating;
    const auto providerName = provider.trimmed();
    if (!providerName.isEmpty()) values << providerName;
    return values;
}

} // namespace CloudStream
