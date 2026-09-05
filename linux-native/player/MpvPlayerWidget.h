#pragma once

#include "SourceCatalog.h"

#include <QList>
#include <QOpenGLWidget>
#include <QString>

struct mpv_handle;
struct mpv_render_context;

namespace CloudStream {

struct MpvTrack {
    int id = -1;
    QString type;
    QString title;
    QString language;
    QString codec;
    bool selected = false;
    bool external = false;
};

class MpvPlayerWidget final : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit MpvPlayerWidget(QWidget *parent = nullptr);
    ~MpvPlayerWidget() override;

    bool isAvailable() const;
    QString initializationError() const;
    double position() const;
    double duration() const;
    bool isPaused() const;
    bool isMuted() const;
    int volume() const;
    double playbackSpeed() const;
    bool fillMode() const;
    int renderedFrameCount() const;
    QString currentVideoOutput() const;
    QList<MpvTrack> tracks() const;

    void loadSource(const PlaybackSource &source,
                    const QList<PlaybackSubtitle> &subtitles = {},
                    double resumePositionSeconds = 0.0);
    void setPaused(bool paused);
    void togglePaused();
    void seekTo(double seconds);
    void seekBy(double seconds);
    void setVolume(int volume);
    void setMuted(bool muted);
    void setPlaybackSpeed(double speed);
    void setFillMode(bool fill);
    void selectSubtitle(int index);
    void setAudioTrack(int id);
    void setSubtitleTrack(int id);

signals:
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void pausedChanged(bool paused);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void loadingChanged(bool loading);
    void playbackError(const QString &message);
    void endReached();
    void fileLoaded();
    void tracksChanged(const QList<CloudStream::MpvTrack> &tracks);

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    enum PropertyId : quint64 {
        PositionProperty = 1,
        DurationProperty,
        PauseProperty,
        VolumeProperty,
        MuteProperty,
        TrackListProperty,
    };

    static void *getProcAddress(void *context, const char *name);
    static void wakeup(void *context);
    static void renderUpdate(void *context);

    void processEvents();
    void executePendingLoad();
    void command(const QStringList &arguments);
    void addSubtitles();

    mpv_handle *handle{};
    mpv_render_context *renderContext{};
    PlaybackSource pendingSource;
    QList<PlaybackSubtitle> pendingSubtitles;
    bool hasPendingSource = false;
    bool externalAudioAdded = false;
    bool available = false;
    QString errorText;
    double currentPosition = 0.0;
    double currentDuration = 0.0;
    double resumePosition = 0.0;
    bool paused = false;
    bool currentMuted = false;
    int currentVolume = 80;
    double currentPlaybackSpeed = 1.0;
    bool currentFillMode = false;
    int frameCount = 0;
    QList<MpvTrack> currentTracks;
};

} // namespace CloudStream
