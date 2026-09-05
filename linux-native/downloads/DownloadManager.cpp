#include "DownloadManager.h"
#include "ProcessSuspension.h"
#include "../app/ProcessCompletion.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <algorithm>


namespace CloudStream {
namespace {
QString cleanTitle(QString value) {
    value.replace(QRegularExpression("[\\\\/:*?\"<>|\\x00-\\x1f]+"), " ");
    value = value.simplified();
    while (value.endsWith('.')) value.chop(1);
    if (value.isEmpty()) value = "Download";
    return value.left(120).trimmed();
}

QString mediaSuffix(const QUrl &url) {
    const auto suffix = QFileInfo(url.path()).suffix().toLower();
    static const QSet<QString> supported{
        "3gp", "aac", "avi", "flac", "flv", "m2ts", "m4a", "m4v", "mkv",
        "mov", "mp3", "mp4", "mpeg", "mpg", "ogg", "ogv", "opus", "ts",
        "wav", "webm", "wmv",
    };
    return supported.contains(suffix) ? suffix : QStringLiteral("mp4");
}

bool hasHeader(const QMap<QString, QString> &headers, const QString &name) {
    for (auto iterator = headers.cbegin(); iterator != headers.cend(); ++iterator) {
        if (iterator.key().compare(name, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

QString ffmpegHeaders(const QMap<QString, QString> &headers) {
    QString block;
    static const QRegularExpression validName("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$");
    for (auto iterator = headers.cbegin(); iterator != headers.cend(); ++iterator) {
        const auto name = iterator.key().trimmed();
        auto value = iterator.value();
        value.remove('\r');
        value.remove('\n');
        if (!validName.match(name).hasMatch() || value.isEmpty()) continue;
        block += name + ": " + value + "\r\n";
    }
    if (!hasHeader(headers, "User-Agent")) block += "User-Agent: CloudStream-Linux/0.1\r\n";
    return block;
}

QString usefulProcessError(const QByteArray &stderrOutput, const QString &fallback) {
    auto text = QString::fromUtf8(stderrOutput).trimmed();
    if (text.size() > 2000) text = text.right(2000);
    return text.isEmpty() ? fallback : text;
}
} // namespace

DownloadManager::DownloadManager(DownloadQueueStore *store, QObject *parent,
                                 QNetworkAccessManager *network)
    : QObject(parent), store_(store), network_(network) {
    if (!network_) {
        network_ = new QNetworkAccessManager(this);
    }
}

DownloadManager::~DownloadManager() {
    if (activeReply_) {
        activeReply_->disconnect(this);
        activeReply_->abort();
    }
    if (activeProcess_) {
        activeProcess_->disconnect(this);
        if (adaptiveSuspended_ &&
            !ProcessSuspension::setSuspended(activeProcess_, false)) activeProcess_->kill();
        activeProcess_->terminate();
        if (!activeProcess_->waitForFinished(1000)) {
            activeProcess_->kill();
            activeProcess_->waitForFinished(1000);
        }
    }
    if (activeFile_.isOpen()) activeFile_.close();
    if (store_ && !activeId_.isEmpty()) {
        auto entry = store_->entry(activeId_);
        if (entry && entry->state == DownloadState::Downloading) {
            entry->state = DownloadState::Paused;
            entry->bytesReceived = QFileInfo(entry->partPath()).size();
            store_->upsert(*entry);
        }
    }
}

bool DownloadManager::isDirectDownload(const PlaybackSource &source) {
    const QUrl url(source.url);
    const auto scheme = url.scheme().toLower();
    return url.isValid() && (scheme == "http" || scheme == "https") &&
           source.type.compare("VIDEO", Qt::CaseInsensitive) == 0;
}

bool DownloadManager::isAdaptiveDownload(const PlaybackSource &source) {
    const QUrl url(source.url);
    const auto scheme = url.scheme().toLower();
    const auto type = source.type.trimmed().toUpper();
    return url.isValid() && (scheme == "http" || scheme == "https") &&
           (type == "M3U8" || type == "DASH");
}

bool DownloadManager::isDownloadable(const PlaybackSource &source) {
    return isDirectDownload(source) || isAdaptiveDownload(source);
}

QString DownloadManager::uniqueTarget(const QString &directory, const QString &title,
                                      const QUrl &url, const QString &forcedSuffix) const {
    const auto base = cleanTitle(title);
    const auto suffix = forcedSuffix.isEmpty() ? mediaSuffix(url) : forcedSuffix;
    const auto usedByQueue = [this](const QString &path) {
        if (!store_) return false;
        for (const auto &entry : store_->entries()) {
            if (QFileInfo(entry.targetPath).absoluteFilePath() == QFileInfo(path).absoluteFilePath()) return true;
        }
        return false;
    };
    for (int number = 1; ; ++number) {
        const auto numbered = number == 1 ? base : base + " (" + QString::number(number) + ")";
        const auto path = QDir(directory).filePath(numbered + "." + suffix);
        if (!QFileInfo::exists(path) && !QFileInfo::exists(path + ".part") && !usedByQueue(path)) return path;
    }
}

QString DownloadManager::enqueue(const QString &title, const PlaybackSource &source,
                                 const QString &downloadDirectory, QString *error,
                                 const DownloadOrigin &origin) {
    if (error) error->clear();
    if (!store_) {
        if (error) *error = "Download queue storage is unavailable";
        return {};
    }
    if (!isDownloadable(source)) {
        if (error) *error = "This source protocol cannot be saved offline";
        return {};
    }
    if (!QDir().mkpath(downloadDirectory) || !QFileInfo(downloadDirectory).isWritable()) {
        if (error) *error = "The selected download folder is not writable";
        return {};
    }
    DownloadEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.title = title.trimmed().isEmpty() ? QStringLiteral("Download") : title.trimmed();
    entry.url = source.url;
    entry.targetPath = uniqueTarget(downloadDirectory, entry.title, QUrl(source.url),
                                    isAdaptiveDownload(source) ? QStringLiteral("mkv") : QString());
    entry.artifactPath = origin.artifactPath;
    entry.provider = origin.provider;
    entry.playbackData = origin.playbackData;
    entry.sourceName = source.name;
    entry.sourceHoster = source.hosterName();
    entry.sourceQuality = source.quality;
    entry.sourceType = source.type.toUpper();
    entry.headers = source.httpHeaders();
    entry.state = DownloadState::Queued;
    if (!store_->upsert(entry)) {
        if (error) *error = store_->errorString();
        return {};
    }
    emit queueChanged();
    emit message("Queued " + entry.title);
    return entry.id;
}

void DownloadManager::start() {
    QTimer::singleShot(0, this, &DownloadManager::startNext);
}

void DownloadManager::startNext() {
    if (!store_ || activeReply_ || activeProcess_) return;
    const auto values = store_->entries();
    const auto found = std::find_if(values.cbegin(), values.cend(),
                                    [](const auto &entry) { return entry.state == DownloadState::Queued; });
    if (found == values.cend()) return;
    begin(*found);
}

void DownloadManager::begin(const DownloadEntry &queued) {
    DownloadEntry entry = queued;
    activeId_ = entry.id;
    activeOffset_ = QFileInfo(entry.partPath()).exists() ? QFileInfo(entry.partPath()).size() : 0;
    outputReady_ = false;
    pauseRequested_ = false;
    removeRequested_ = false;
    deleteFilesOnRemove_ = false;
    activeFailure_.clear();
    adaptiveProgressBuffer_.clear();
    adaptiveErrorOutput_.clear();
    adaptiveSuspended_ = false;
    persistClock_.invalidate();

    entry.state = DownloadState::Downloading;
    entry.error.clear();
    entry.bytesReceived = activeOffset_;
    store_->upsert(entry);
    emit queueChanged();
    emit message("Downloading " + entry.title);

    PlaybackSource source;
    source.url = entry.url;
    source.type = entry.sourceType;
    if (isAdaptiveDownload(source)) {
        beginAdaptive(entry);
        return;
    }

    QNetworkRequest request(QUrl(entry.url));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    for (auto iterator = entry.headers.cbegin(); iterator != entry.headers.cend(); ++iterator) {
        request.setRawHeader(iterator.key().toUtf8(), iterator.value().toUtf8());
    }
    if (!hasHeader(entry.headers, "User-Agent")) {
        request.setRawHeader("User-Agent", "CloudStream-Linux/0.1");
    }
    if (activeOffset_ > 0) request.setRawHeader("Range", "bytes=" + QByteArray::number(activeOffset_) + "-");

    auto *reply = network_->get(request);
    activeReply_ = reply;
    connect(reply, &QNetworkReply::metaDataChanged, this, [this, reply] { openOutput(reply); });
    connect(reply, &QIODevice::readyRead, this, [this, reply] { consumeReadyData(reply); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, reply](qint64 received, qint64 total) {
        if (reply != activeReply_) return;
        const auto absoluteReceived = std::max<qint64>(0, activeOffset_ + received);
        const auto absoluteTotal = total > 0 ? activeOffset_ + total : qint64(0);
        persistProgress(absoluteReceived, absoluteTotal);
        emit progressChanged(activeId_, absoluteReceived, absoluteTotal);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] { finish(reply); });
}

void DownloadManager::beginAdaptive(const DownloadEntry &entry) {
    QFile::remove(entry.partPath());
    activeOffset_ = 0;
    if (auto current = store_->entry(entry.id)) {
        current->bytesReceived = 0;
        current->bytesTotal = 0;
        store_->upsert(*current);
    }

    auto *process = new QProcess(this);
    activeProcess_ = process;
    process->setProcessChannelMode(QProcess::SeparateChannels);
    auto ffmpeg = QStandardPaths::findExecutable("ffmpeg");
#ifdef Q_OS_WIN
    const auto bundledFfmpeg = QDir(QCoreApplication::applicationDirPath()).filePath("ffmpeg.exe");
    if (QFileInfo(bundledFfmpeg).isFile()) ffmpeg = bundledFfmpeg;
#endif
    if (ffmpeg.isEmpty()) activeFailure_ = "FFmpeg is required to save HLS and DASH streams";
    process->setProgram(ffmpeg.isEmpty() ? QStringLiteral("ffmpeg") : ffmpeg);
    QStringList arguments{
        "-hide_banner", "-loglevel", "warning", "-nostdin", "-y",
        "-progress", "pipe:1",
    };
    const auto headers = ffmpegHeaders(entry.headers);
    if (!headers.isEmpty()) arguments << "-headers" << headers;
    arguments << "-i" << entry.url
              << "-map" << "0:v?" << "-map" << "0:a?" << "-map" << "0:s?"
              << "-c" << "copy" << "-ignore_unknown" << "-f" << "matroska"
              << entry.partPath();
    process->setArguments(arguments);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        if (process != activeProcess_) return;
        adaptiveProgressBuffer_ += process->readAllStandardOutput();
        for (;;) {
            const auto newline = adaptiveProgressBuffer_.indexOf('\n');
            if (newline < 0) break;
            const auto line = adaptiveProgressBuffer_.left(newline).trimmed();
            adaptiveProgressBuffer_.remove(0, newline + 1);
            if (!line.startsWith("total_size=")) continue;
            bool ok = false;
            const auto bytes = line.mid(sizeof("total_size=") - 1).toLongLong(&ok);
            if (!ok) continue;
            persistProgress(bytes, 0);
            emit progressChanged(activeId_, bytes, 0);
        }
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        if (process != activeProcess_) return;
        adaptiveErrorOutput_ += process->readAllStandardError();
        if (adaptiveErrorOutput_.size() > 65536) {
            adaptiveErrorOutput_ = adaptiveErrorOutput_.right(65536);
        }
    });
    ProcessCompletion::watch(
        process, this,
        [this, process](int exitCode, QProcess::ExitStatus exitStatus, bool failedToStart) {
            finishAdaptive(process, exitCode, exitStatus, failedToStart);
        });
    process->start();
}

bool DownloadManager::openOutput(QNetworkReply *reply) {
    if (!reply || reply != activeReply_ || outputReady_ || !store_) return outputReady_;
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status <= 0) return false;
    if (status >= 400) {
        activeFailure_ = "Server returned HTTP " + QString::number(status);
        reply->abort();
        return false;
    }
    auto entry = store_->entry(activeId_);
    if (!entry) {
        activeFailure_ = "Download disappeared from the queue";
        reply->abort();
        return false;
    }
    const bool resumed = activeOffset_ > 0 && status == 206;
    if (!resumed) activeOffset_ = 0;
    activeFile_.setFileName(entry->partPath());
    const auto mode = QIODevice::WriteOnly | (resumed ? QIODevice::Append : QIODevice::Truncate);
    if (!activeFile_.open(mode)) {
        activeFailure_ = activeFile_.errorString();
        reply->abort();
        return false;
    }
    outputReady_ = true;
    const auto contentLength = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    qint64 total = contentLength > 0 ? activeOffset_ + contentLength : 0;
    const auto contentRange = QString::fromLatin1(reply->rawHeader("Content-Range"));
    const QRegularExpression rangeExpression("/([0-9]+)$");
    const auto rangeMatch = rangeExpression.match(contentRange);
    if (rangeMatch.hasMatch()) total = rangeMatch.captured(1).toLongLong();
    persistProgress(activeOffset_, total, true);
    return true;
}

void DownloadManager::consumeReadyData(QNetworkReply *reply) {
    if (!reply || reply != activeReply_) return;
    if (!outputReady_ && !openOutput(reply)) return;
    const auto bytes = reply->readAll();
    if (bytes.isEmpty()) return;
    if (activeFile_.write(bytes) != bytes.size()) {
        activeFailure_ = activeFile_.errorString().isEmpty()
            ? QStringLiteral("Could not write the downloaded media") : activeFile_.errorString();
        reply->abort();
    }
}

void DownloadManager::persistProgress(qint64 received, qint64 total, bool force) {
    if (!store_ || activeId_.isEmpty()) return;
    if (!force && persistClock_.isValid() && persistClock_.elapsed() < 1000) return;
    auto entry = store_->entry(activeId_);
    if (!entry) return;
    entry->bytesReceived = std::max(received, QFileInfo(entry->partPath()).size());
    if (total > 0) entry->bytesTotal = total;
    store_->upsert(*entry);
    if (!persistClock_.isValid()) persistClock_.start();
    else persistClock_.restart();
}

void DownloadManager::clearActive() {
    activeReply_ = nullptr;
    activeProcess_ = nullptr;
    activeFile_.setFileName({});
    activeId_.clear();
    activeOffset_ = 0;
    outputReady_ = false;
    pauseRequested_ = false;
    removeRequested_ = false;
    deleteFilesOnRemove_ = false;
    activeFailure_.clear();
    adaptiveProgressBuffer_.clear();
    adaptiveErrorOutput_.clear();
    adaptiveSuspended_ = false;
    persistClock_.invalidate();
}

void DownloadManager::finish(QNetworkReply *reply) {
    if (!reply || reply != activeReply_ || !store_) {
        if (reply) reply->deleteLater();
        return;
    }
    if (reply->error() == QNetworkReply::NoError) consumeReadyData(reply);
    if (activeFile_.isOpen()) {
        activeFile_.flush();
        activeFile_.close();
    }
    const auto id = activeId_;
    auto entry = store_->entry(id);
    const bool removeRequested = removeRequested_;
    const bool deleteFiles = deleteFilesOnRemove_;
    const bool pauseRequested = pauseRequested_;
    const auto failure = activeFailure_;
    const auto networkError = reply->errorString();
    reply->deleteLater();
    clearActive();

    if (!entry) {
        startNext();
        return;
    }
    entry->bytesReceived = QFileInfo(entry->partPath()).size();
    if (removeRequested) {
        if (deleteFiles) {
            QFile::remove(entry->partPath());
            QFile::remove(entry->targetPath);
        }
        store_->remove(id);
        emit queueChanged();
        startNext();
        return;
    }
    if (pauseRequested) {
        entry->state = DownloadState::Paused;
        entry->error.clear();
        store_->upsert(*entry);
        emit queueChanged();
        emit message("Paused " + entry->title);
        startNext();
        return;
    }
    if (!failure.isEmpty() || reply->error() != QNetworkReply::NoError) {
        entry->state = DownloadState::Failed;
        entry->error = !failure.isEmpty() ? failure : networkError;
        store_->upsert(*entry);
        emit queueChanged();
        emit message("Download failed: " + entry->title);
        startNext();
        return;
    }
    if (QFileInfo::exists(entry->targetPath)) {
        entry->state = DownloadState::Failed;
        entry->error = "The destination file already exists";
        store_->upsert(*entry);
        emit queueChanged();
        startNext();
        return;
    }
    if (!QFile::rename(entry->partPath(), entry->targetPath)) {
        entry->state = DownloadState::Failed;
        entry->error = "Could not activate the completed download";
        store_->upsert(*entry);
        emit queueChanged();
        startNext();
        return;
    }
    const auto finalSize = QFileInfo(entry->targetPath).size();
    entry->state = DownloadState::Completed;
    entry->bytesReceived = finalSize;
    if (entry->bytesTotal <= 0) entry->bytesTotal = finalSize;
    entry->error.clear();
    store_->upsert(*entry);
    emit progressChanged(id, finalSize, entry->bytesTotal);
    emit queueChanged();
    emit message("Downloaded " + entry->title);
    startNext();
}

void DownloadManager::finishAdaptive(QProcess *process, int exitCode,
                                     QProcess::ExitStatus exitStatus, bool failedToStart) {
    if (!process || process != activeProcess_ || !store_) {
        if (process) process->deleteLater();
        return;
    }
    adaptiveProgressBuffer_ += process->readAllStandardOutput();
    adaptiveErrorOutput_ += process->readAllStandardError();
    const auto id = activeId_;
    auto entry = store_->entry(id);
    const bool removeRequested = removeRequested_;
    const bool deleteFiles = deleteFilesOnRemove_;
    const auto configuredFailure = activeFailure_;
    const auto processError = process->errorString();
    const auto stderrOutput = adaptiveErrorOutput_;
    process->deleteLater();
    clearActive();

    if (!entry) {
        startNext();
        return;
    }
    entry->bytesReceived = QFileInfo(entry->partPath()).size();
    if (removeRequested) {
        if (deleteFiles) {
            QFile::remove(entry->partPath());
            QFile::remove(entry->targetPath);
        }
        store_->remove(id);
        emit queueChanged();
        startNext();
        return;
    }
    if (failedToStart || exitStatus != QProcess::NormalExit || exitCode != 0 ||
        entry->bytesReceived <= 0) {
        entry->state = DownloadState::Failed;
        const auto fallback = !configuredFailure.isEmpty()
            ? configuredFailure
            : (failedToStart ? processError : QStringLiteral("FFmpeg could not save this stream"));
        entry->error = usefulProcessError(stderrOutput, fallback);
        store_->upsert(*entry);
        emit queueChanged();
        emit message("Download failed: " + entry->title);
        startNext();
        return;
    }
    if (QFileInfo::exists(entry->targetPath)) {
        entry->state = DownloadState::Failed;
        entry->error = "The destination file already exists";
        store_->upsert(*entry);
        emit queueChanged();
        startNext();
        return;
    }
    if (!QFile::rename(entry->partPath(), entry->targetPath)) {
        entry->state = DownloadState::Failed;
        entry->error = "Could not activate the completed download";
        store_->upsert(*entry);
        emit queueChanged();
        startNext();
        return;
    }
    const auto finalSize = QFileInfo(entry->targetPath).size();
    entry->state = DownloadState::Completed;
    entry->bytesReceived = finalSize;
    entry->bytesTotal = finalSize;
    entry->error.clear();
    store_->upsert(*entry);
    emit progressChanged(id, finalSize, finalSize);
    emit queueChanged();
    emit message("Downloaded " + entry->title);
    startNext();
}

bool DownloadManager::pause(const QString &id) {
    if (!store_) return false;
    auto entry = store_->entry(id);
    if (!entry) return false;
    if (id == activeId_ && activeReply_) {
        pauseRequested_ = true;
        activeReply_->abort();
        return true;
    }
    if (id == activeId_ && activeProcess_) {
        if (adaptiveSuspended_ || removeRequested_) return false;
        QString error;
        if (!ProcessSuspension::setSuspended(activeProcess_, true, &error)) {
            emit message("Could not pause download: " + error);
            return false;
        }
        adaptiveSuspended_ = true;
        entry->state = DownloadState::Paused;
        entry->error.clear();
        entry->bytesReceived = QFileInfo(entry->partPath()).size();
        const auto saved = store_->upsert(*entry);
        if (saved) {
            emit queueChanged();
            emit message("Paused " + entry->title);
        }
        return saved;
    }
    if (entry->state != DownloadState::Queued && entry->state != DownloadState::Downloading) return false;
    entry->state = DownloadState::Paused;
    entry->error.clear();
    const auto saved = store_->upsert(*entry);
    if (saved) emit queueChanged();
    return saved;
}

bool DownloadManager::resume(const QString &id) {
    if (!store_) return false;
    auto entry = store_->entry(id);
    if (!entry || entry->state == DownloadState::Downloading) return false;
    if (id == activeId_ && activeProcess_ && adaptiveSuspended_) {
        if (removeRequested_) return false;
        QString error;
        if (!ProcessSuspension::setSuspended(activeProcess_, false, &error)) {
            emit message("Could not resume download: " + error);
            return false;
        }
        adaptiveSuspended_ = false;
        entry->state = DownloadState::Downloading;
        entry->error.clear();
        const auto saved = store_->upsert(*entry);
        if (saved) {
            emit queueChanged();
            emit message("Downloading " + entry->title);
        }
        return saved;
    }
    if (entry->state == DownloadState::Completed) {
        if (QFileInfo::exists(entry->targetPath)) return false;
        QFile::remove(entry->partPath());
        entry->bytesReceived = 0;
        entry->bytesTotal = 0;
    }
    entry->state = DownloadState::Queued;
    entry->error.clear();
    const auto saved = store_->upsert(*entry);
    if (saved) {
        emit queueChanged();
        start();
    }
    return saved;
}

bool DownloadManager::remove(const QString &id, bool deleteFiles) {
    if (!store_) return false;
    auto entry = store_->entry(id);
    if (!entry) return false;
    if (id == activeId_ && activeReply_) {
        removeRequested_ = true;
        deleteFilesOnRemove_ = deleteFiles;
        activeReply_->abort();
        return true;
    }
    if (id == activeId_ && activeProcess_) {
        removeRequested_ = true;
        deleteFilesOnRemove_ = deleteFiles;
        if (adaptiveSuspended_) {
            if (!ProcessSuspension::setSuspended(activeProcess_, false)) activeProcess_->kill();
            adaptiveSuspended_ = false;
        }
        auto *process = activeProcess_.data();
        process->terminate();
        QTimer::singleShot(1000, process, [process] {
            if (process->state() != QProcess::NotRunning) process->kill();
        });
        return true;
    }
    if (deleteFiles) {
        QFile::remove(entry->partPath());
        QFile::remove(entry->targetPath);
    }
    const auto removed = store_->remove(id);
    if (removed) emit queueChanged();
    return removed;
}

bool DownloadManager::updateSource(const QString &id, const PlaybackSource &source) {
    if (!store_ || id == activeId_ || !isDownloadable(source)) return false;
    auto entry = store_->entry(id);
    if (!entry) return false;
    entry->url = source.url;
    entry->headers = source.httpHeaders();
    entry->sourceName = source.name;
    entry->sourceHoster = source.hosterName();
    entry->sourceQuality = source.quality;
    entry->sourceType = source.type.toUpper();
    entry->error.clear();
    const auto saved = store_->upsert(*entry);
    if (saved) emit queueChanged();
    return saved;
}

} // namespace CloudStream
