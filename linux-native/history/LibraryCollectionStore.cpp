#include "LibraryCollectionStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <utility>

namespace CloudStream {
namespace {
bool nameTaken(const QList<LibraryCollection> &collections, const QString &name,
               const QString &exceptId = {}) {
    return std::any_of(collections.cbegin(), collections.cend(), [&](const auto &value) {
        return value.id != exceptId && value.name.compare(name, Qt::CaseInsensitive) == 0;
    });
}
} // namespace

LibraryCollectionStore::LibraryCollectionStore(QString filePath) : filePath(std::move(filePath)) {}

bool LibraryCollectionStore::read(Data &data) const {
    QFile file(filePath);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
    const auto root = document.object();
    if (!root.value("collections").isArray()) return false;
    data.selectedId = root.value("selectedId").toString();
    for (const auto &value : root.value("collections").toArray()) {
        if (!value.isObject()) continue;
        const auto object = value.toObject();
        LibraryCollection collection;
        collection.id = object.value("id").toString();
        collection.name = object.value("name").toString();
        for (const auto &member : object.value("itemIds").toArray()) {
            if (member.isString() && !member.toString().isEmpty() &&
                !collection.itemIds.contains(member.toString())) collection.itemIds << member.toString();
        }
        if (!collection.id.isEmpty() && !collection.name.isEmpty()) data.collections << collection;
    }
    return true;
}

bool LibraryCollectionStore::write(const Data &data) const {
    if (!QDir().mkpath(QFileInfo(filePath).absolutePath())) return false;
    QJsonArray values;
    for (const auto &collection : data.collections) {
        QJsonArray members;
        for (const auto &id : collection.itemIds) members.append(id);
        values.append(QJsonObject{{"id", collection.id}, {"name", collection.name},
                                  {"itemIds", members}});
    }
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const auto bytes = QJsonDocument(QJsonObject{{"selectedId", data.selectedId},
                                                  {"collections", values}}).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size()) return false;
    return file.commit();
}

QList<LibraryCollection> LibraryCollectionStore::collections() const {
    Data data;
    return read(data) ? data.collections : QList<LibraryCollection>{};
}

QString LibraryCollectionStore::selectedId() const {
    Data data;
    if (!read(data)) return {};
    const auto it = std::find_if(data.collections.cbegin(), data.collections.cend(), [&](const auto &value) {
        return value.id == data.selectedId;
    });
    return it == data.collections.cend() ? QString{} : data.selectedId;
}

QString LibraryCollectionStore::create(const QString &name) {
    const auto clean = name.trimmed();
    Data data;
    if (clean.isEmpty() || !read(data) || nameTaken(data.collections, clean)) return {};
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    data.collections << LibraryCollection{id, clean, {}};
    return write(data) ? id : QString{};
}

bool LibraryCollectionStore::select(const QString &id) {
    Data data;
    if (!read(data)) return false;
    if (!id.isEmpty() && std::none_of(data.collections.cbegin(), data.collections.cend(), [&](const auto &value) {
        return value.id == id;
    })) return false;
    data.selectedId = id;
    return write(data);
}

bool LibraryCollectionStore::rename(const QString &id, const QString &name) {
    const auto clean = name.trimmed();
    Data data;
    if (clean.isEmpty() || !read(data) || nameTaken(data.collections, clean, id)) return false;
    for (auto &value : data.collections) {
        if (value.id == id) { value.name = clean; return write(data); }
    }
    return false;
}

bool LibraryCollectionStore::removeCollection(const QString &id) {
    Data data;
    if (id.isEmpty() || !read(data)) return false;
    const auto oldSize = data.collections.size();
    data.collections.erase(std::remove_if(data.collections.begin(), data.collections.end(), [&](const auto &value) {
        return value.id == id;
    }), data.collections.end());
    if (oldSize == data.collections.size()) return false;
    if (data.selectedId == id) data.selectedId.clear();
    return write(data);
}

bool LibraryCollectionStore::add(const QString &id, const QString &itemId) {
    Data data;
    if (itemId.trimmed().isEmpty() || !read(data)) return false;
    for (auto &value : data.collections) {
        if (value.id == id) {
            if (value.itemIds.contains(itemId)) return false;
            value.itemIds << itemId;
            return write(data);
        }
    }
    return false;
}

bool LibraryCollectionStore::removeItem(const QString &id, const QString &itemId) {
    Data data;
    if (!read(data)) return false;
    for (auto &value : data.collections) {
        if (value.id == id) {
            if (!value.itemIds.removeOne(itemId)) return false;
            return write(data);
        }
    }
    return false;
}

bool LibraryCollectionStore::contains(const QString &id, const QString &itemId) const {
    Data data;
    if (!read(data)) return false;
    for (const auto &value : data.collections) {
        if (value.id == id) return value.itemIds.contains(itemId);
    }
    return false;
}

} // namespace CloudStream
