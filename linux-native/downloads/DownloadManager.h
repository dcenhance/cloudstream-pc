#pragma once

#include "DownloadQueueStore.h"
#include "../player/SourceCatalog.h"

#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QProcess>

class QNetworkAccessManager;
class QNetworkReply;
namespace CloudStream {

struct DownloadOrigin {
    QString artifactPath;
    QString provider;
    QString playbackData;
};

class DownloadManager final : public QObject {
    Q_OBJECT
public:
    explicit DownloadManager(DownloadQueueStore *store, QObject *parent = nullptr,
                             QNetworkAccessManager *network = nullptr);
    ~DownloadManager() override;

    QString enqueue(const QString &title, const PlaybackSource &source,
                    const QString &downloadDirectory, QString *error = nullptr,
                    const DownloadOrigin &origin = {});
    void start();
    bool pause(const QString &id);
    bool resume(const QString &id);
    bool remove(const QString &id, bool deleteFiles);
    bool updateSource(const QString &id, const PlaybackSource &source);

    static bool isDirectDownload(const PlaybackSource &source);
    static bool isAdaptiveDownload(const PlaybackSource &source);
    static bool isDownloadable(const PlaybackSource &source);

signals:
    void queueChanged();
    void progressChanged(const QString &id, qint64 received, qint64 total);
    void message(const QString &text);

private:
    void startNext();
    void begin(const DownloadEntry &entry);
    void beginAdaptive(const DownloadEntry &entry);
    bool openOutput(QNetworkReply *reply);
    void consumeReadyData(QNetworkReply *reply);
    void finish(QNetworkReply *reply);
    void finishAdaptive(QProcess *process, int exitCode, QProcess::ExitStatus exitStatus,
                        bool failedToStart);
    void persistProgress(qint64 received, qint64 total, bool force = false);
    QString uniqueTarget(const QString &directory, const QString &title,
                         const QUrl &url, const QString &forcedSuffix = {}) const;
    void clearActive();

    DownloadQueueStore *store_{};
    QNetworkAccessManager *network_{};
    QPointer<QNetworkReply> activeReply_;
    QPointer<QProcess> activeProcess_;
    QFile activeFile_;
    QString activeId_;
    qint64 activeOffset_ = 0;
    bool outputReady_ = false;
    bool pauseRequested_ = false;
    bool removeRequested_ = false;
    bool deleteFilesOnRemove_ = false;
    QString activeFailure_;
    QByteArray adaptiveProgressBuffer_;
    QByteArray adaptiveErrorOutput_;
    bool adaptiveSuspended_ = false;
    QElapsedTimer persistClock_;
};

} // namespace CloudStream
