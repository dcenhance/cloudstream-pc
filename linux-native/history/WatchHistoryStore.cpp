#include "WatchHistoryStore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

namespace CloudStream {
namespace {
QJsonObject toJson(const WatchEntry &entry) {
    return {
        {"id", entry.id},
        {"name", entry.name},
        {"sourceUrl", entry.sourceUrl},
        {"provider", entry.provider},
        {"jarPath", entry.jarPath},
        {"posterUrl", entry.posterUrl},
        {"playbackData", entry.playbackData},
        {"episodeName", entry.episodeName},
        {"state", entry.state},
        {"positionSeconds", entry.positionSeconds},
        {"durationSeconds", entry.durationSeconds},
        {"updatedAt", entry.updatedAt},
    };
}

WatchEntry fromJson(const QJsonObject &object) {
    WatchEntry entry;
    entry.id = object.value("id").toString();
    entry.name = object.value("name").toString();
    entry.sourceUrl = object.value("sourceUrl").toString();
    entry.provider = object.value("provider").toString();
    entry.jarPath = object.value("jarPath").toString();
    entry.posterUrl = object.value("posterUrl").toString();
    entry.playbackData = object.value("playbackData").toString();
    entry.episodeName = object.value("episodeName").toString();
    entry.state = object.value("state").toString("Watching");
    entry.positionSeconds = object.value("positionSeconds").toDouble();
    entry.durationSeconds = object.value("durationSeconds").toDouble();
    entry.updatedAt = object.value("updatedAt").toInteger();
    return entry;
}
} // namespace

WatchHistoryStore::WatchHistoryStore(QString filePath) : filePath(std::move(filePath)) {}

QString WatchHistoryStore::idFor(const QString &provider, const QString &sourceUrl) {
    return QString::fromLatin1(QCryptographicHash::hash(
        provider.toUtf8() + '\n' + sourceUrl.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QList<WatchEntry> WatchHistoryStore::readEntries() const {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) return {};
    QList<WatchEntry> result;
    for (const auto &value : document.array()) {
        if (!value.isObject()) continue;
        auto entry = fromJson(value.toObject());
        if (!entry.id.isEmpty()) result << entry;
    }
    return result;
}

QList<WatchEntry> WatchHistoryStore::entries(const QString &state) const {
    auto result = readEntries();
    if (!state.isEmpty()) {
        result.erase(std::remove_if(result.begin(), result.end(), [&state](const WatchEntry &entry) {
            return entry.state.compare(state, Qt::CaseInsensitive) != 0;
        }), result.end());
    }
    std::sort(result.begin(), result.end(), [](const WatchEntry &left, const WatchEntry &right) {
        return left.updatedAt > right.updatedAt;
    });
    return result;
}

bool WatchHistoryStore::writeEntries(const QList<WatchEntry> &entries) const {
    if (!QDir().mkpath(QFileInfo(filePath).absolutePath())) return false;
    QJsonArray values;
    for (const auto &entry : entries) values.append(toJson(entry));
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(QJsonDocument(values).toJson(QJsonDocument::Compact)) < 0) return false;
    return file.commit();
}

bool WatchHistoryStore::upsert(const WatchEntry &entry) {
    if (entry.id.isEmpty() || entry.name.trimmed().isEmpty()) return false;
    auto values = readEntries();
    auto saved = entry;
    if (saved.updatedAt <= 0) saved.updatedAt = QDateTime::currentMSecsSinceEpoch();
    const auto existing = std::find_if(values.begin(), values.end(), [&saved](const WatchEntry &value) {
        return value.id == saved.id;
    });
    if (existing == values.end()) values << saved;
    else *existing = saved;
    return writeEntries(values);
}

bool WatchHistoryStore::updateProgress(const QString &id, double positionSeconds,
                                       double durationSeconds, qint64 updatedAt) {
    auto values = readEntries();
    const auto existing = std::find_if(values.begin(), values.end(), [&id](const WatchEntry &entry) {
        return entry.id == id;
    });
    if (existing == values.end()) return false;
    existing->durationSeconds = std::max(0.0, durationSeconds);
    existing->positionSeconds = std::max(0.0, positionSeconds);
    if (existing->durationSeconds > 0.0) {
        existing->positionSeconds = std::min(existing->positionSeconds, existing->durationSeconds);
    }
    const auto completed = existing->durationSeconds > 0.0 &&
        existing->positionSeconds / existing->durationSeconds >= 0.9;
    existing->state = completed ? "Completed" : "Watching";
    existing->updatedAt = updatedAt > 0 ? updatedAt : QDateTime::currentMSecsSinceEpoch();
    return writeEntries(values);
}

bool WatchHistoryStore::setState(const QString &id, const QString &state) {
    static const QStringList supported{"Watching", "Completed", "Paused", "Cancelled"};
    if (!supported.contains(state)) return false;
    auto values = readEntries();
    const auto existing = std::find_if(values.begin(), values.end(), [&id](const WatchEntry &entry) {
        return entry.id == id;
    });
    if (existing == values.end()) return false;
    existing->state = state;
    existing->updatedAt = QDateTime::currentMSecsSinceEpoch();
    return writeEntries(values);
}

bool WatchHistoryStore::remove(const QString &id) {
    auto values = readEntries();
    const auto oldSize = values.size();
    values.erase(std::remove_if(values.begin(), values.end(), [&id](const WatchEntry &entry) {
        return entry.id == id;
    }), values.end());
    return values.size() != oldSize && writeEntries(values);
}

} // namespace CloudStream
