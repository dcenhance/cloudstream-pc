#include "DownloadQueueStore.h"

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
QJsonObject headersToJson(const QMap<QString, QString> &headers) {
    QJsonObject object;
    for (auto iterator = headers.cbegin(); iterator != headers.cend(); ++iterator) {
        object.insert(iterator.key(), iterator.value());
    }
    return object;
}

QMap<QString, QString> headersFromJson(const QJsonObject &object) {
    QMap<QString, QString> headers;
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        if (iterator.value().isString()) headers.insert(iterator.key(), iterator.value().toString());
    }
    return headers;
}
} // namespace

DownloadQueueStore::DownloadQueueStore(QString path) : path_(std::move(path)) {
    load();
}

QList<DownloadEntry> DownloadQueueStore::entries() const {
    return entries_;
}

std::optional<DownloadEntry> DownloadQueueStore::entry(const QString &id) const {
    const auto found = std::find_if(entries_.cbegin(), entries_.cend(),
                                    [&id](const auto &entry) { return entry.id == id; });
    if (found == entries_.cend()) return std::nullopt;
    return *found;
}

bool DownloadQueueStore::upsert(DownloadEntry entry) {
    error_.clear();
    if (entry.id.trimmed().isEmpty() || entry.url.trimmed().isEmpty() ||
        entry.targetPath.trimmed().isEmpty()) {
        error_ = "Download entry is missing its ID, URL, or target path";
        return false;
    }
    const auto now = QDateTime::currentSecsSinceEpoch();
    if (entry.createdAt <= 0) entry.createdAt = now;
    entry.updatedAt = now;
    entry.bytesReceived = std::max<qint64>(0, entry.bytesReceived);
    entry.bytesTotal = std::max<qint64>(0, entry.bytesTotal);
    if (entry.bytesTotal > 0) entry.bytesReceived = std::min(entry.bytesReceived, entry.bytesTotal);
    const auto found = std::find_if(entries_.begin(), entries_.end(),
                                    [&entry](const auto &candidate) { return candidate.id == entry.id; });
    if (found == entries_.end()) entries_.append(entry);
    else *found = entry;
    return persist();
}

bool DownloadQueueStore::remove(const QString &id) {
    error_.clear();
    const auto oldSize = entries_.size();
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&id](const auto &entry) { return entry.id == id; }),
                   entries_.end());
    if (entries_.size() == oldSize) return false;
    return persist();
}

QString DownloadQueueStore::errorString() const {
    return error_;
}

QString DownloadQueueStore::stateName(DownloadState state) {
    switch (state) {
    case DownloadState::Queued: return "queued";
    case DownloadState::Downloading: return "downloading";
    case DownloadState::Paused: return "paused";
    case DownloadState::Completed: return "completed";
    case DownloadState::Failed: return "failed";
    }
    return "failed";
}

DownloadState DownloadQueueStore::stateFromName(const QString &state) {
    const auto normalized = state.trimmed().toLower();
    if (normalized == "queued") return DownloadState::Queued;
    if (normalized == "downloading") return DownloadState::Downloading;
    if (normalized == "paused") return DownloadState::Paused;
    if (normalized == "completed") return DownloadState::Completed;
    return DownloadState::Failed;
}

bool DownloadQueueStore::load() {
    entries_.clear();
    error_.clear();
    QFile file(path_);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) {
        error_ = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        error_ = "Download queue is not valid JSON: " + parseError.errorString();
        return false;
    }
    const auto values = document.object().value("downloads").toArray();
    for (const auto &value : values) {
        const auto object = value.toObject();
        DownloadEntry entry;
        entry.id = object.value("id").toString();
        entry.title = object.value("title").toString();
        entry.url = object.value("url").toString();
        entry.targetPath = object.value("targetPath").toString();
        entry.artifactPath = object.value("artifactPath").toString();
        entry.provider = object.value("provider").toString();
        entry.playbackData = object.value("playbackData").toString();
        entry.sourceName = object.value("sourceName").toString();
        entry.sourceHoster = object.value("sourceHoster").toString();
        entry.sourceQuality = object.value("sourceQuality").toInt();
        entry.sourceType = object.value("sourceType").toString("VIDEO").toUpper();
        entry.headers = headersFromJson(object.value("headers").toObject());
        entry.state = stateFromName(object.value("state").toString());
        if (entry.state == DownloadState::Downloading) entry.state = DownloadState::Paused;
        entry.bytesReceived = std::max<qint64>(0, object.value("bytesReceived").toVariant().toLongLong());
        entry.bytesTotal = std::max<qint64>(0, object.value("bytesTotal").toVariant().toLongLong());
        entry.createdAt = object.value("createdAt").toVariant().toLongLong();
        entry.updatedAt = object.value("updatedAt").toVariant().toLongLong();
        entry.error = object.value("error").toString();
        if (entry.id.isEmpty() || entry.url.isEmpty() || entry.targetPath.isEmpty()) continue;
        entries_.append(entry);
    }
    return true;
}

bool DownloadQueueStore::persist() {
    const auto directory = QFileInfo(path_).absolutePath();
    if (!QDir().mkpath(directory)) {
        error_ = "Could not create download queue directory";
        return false;
    }
    QJsonArray values;
    for (const auto &entry : entries_) {
        QJsonObject object;
        object.insert("id", entry.id);
        object.insert("title", entry.title);
        object.insert("url", entry.url);
        object.insert("targetPath", entry.targetPath);
        object.insert("artifactPath", entry.artifactPath);
        object.insert("provider", entry.provider);
        object.insert("playbackData", entry.playbackData);
        object.insert("sourceName", entry.sourceName);
        object.insert("sourceHoster", entry.sourceHoster);
        object.insert("sourceQuality", entry.sourceQuality);
        object.insert("sourceType", entry.sourceType);
        object.insert("headers", headersToJson(entry.headers));
        object.insert("state", stateName(entry.state));
        object.insert("bytesReceived", entry.bytesReceived);
        object.insert("bytesTotal", entry.bytesTotal);
        object.insert("createdAt", entry.createdAt);
        object.insert("updatedAt", entry.updatedAt);
        object.insert("error", entry.error);
        values.append(object);
    }
    QJsonObject root;
    root.insert("version", 2);
    root.insert("downloads", values);
    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly)) {
        error_ = file.errorString();
        return false;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const auto payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.commit()) {
        error_ = file.errorString();
        return false;
    }
    if (!QFile::setPermissions(path_, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        error_ = "Could not restrict download queue permissions";
        return false;
    }
    return true;
}

} // namespace CloudStream
