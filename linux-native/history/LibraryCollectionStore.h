#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace CloudStream {

struct LibraryCollection {
    QString id;
    QString name;
    QStringList itemIds;
};

// Collection references point at WatchHistoryStore IDs. Removing a collection or
// membership never removes the corresponding watch-history entries.
class LibraryCollectionStore final {
public:
    explicit LibraryCollectionStore(QString filePath);

    QList<LibraryCollection> collections() const;
    QString selectedId() const; // empty means the entire library
    QString create(const QString &name);
    bool select(const QString &id);
    bool rename(const QString &id, const QString &name);
    bool removeCollection(const QString &id);
    bool add(const QString &id, const QString &itemId);
    bool removeItem(const QString &id, const QString &itemId);
    bool contains(const QString &id, const QString &itemId) const;

private:
    struct Data {
        QString selectedId;
        QList<LibraryCollection> collections;
    };
    QString filePath;
    bool read(Data &data) const;
    bool write(const Data &data) const;
};

} // namespace CloudStream
