#pragma once

#include "SourceCatalog.h"

#include <QDialog>
#include <QRect>
#include <QSet>

class QCloseEvent;
class QComboBox;
class QEvent;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;

namespace CloudStream {

class MpvPlayerWidget;

struct PlayerPreferences {
    int seekSeconds = 10;
    int initialVolume = 80;
    bool automaticFallback = true;
    bool showInformation = true;
    bool selectFirstSubtitle = false;
    int autoHideDelayMs = 3200;
};

class IntegratedPlayerWindow final : public QDialog {
    Q_OBJECT

public:
    explicit IntegratedPlayerWindow(const SourceDiscovery &discovery,
                                    QString title,
                                    double resumePositionSeconds = 0.0,
                                    QWidget *parent = nullptr,
                                    PlayerPreferences preferences = {});

    double position() const;
    double duration() const;
    int currentSourceIndex() const;

signals:
    void progressUpdated(double positionSeconds, double durationSeconds);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static QString formatTime(double seconds);
    void switchSource(int index, bool automaticFallback);
    void handlePlaybackError(const QString &message);
    void toggleFullscreen();
    void refreshTimeLabel();
    void setControlsVisible(bool visible);
    void scheduleAutoHide();
    void showSourceDialog();
    void showTrackDialog();
    void showSpeedDialog();

    SourceDiscovery discovery;
    PlayerPreferences preferences;
    QString mediaTitle;
    MpvPlayerWidget *video{};
    QWidget *chrome{};
    QWidget *topBar{};
    QWidget *controls{};
    QLabel *loadingLabel{};
    QLabel *titleLabel{};
    QLabel *sourceStatus{};
    QLabel *timeLabel{};
    QLabel *durationLabel{};
    QPushButton *playPause{};
    QPushButton *rewind{};
    QPushButton *forward{};
    QPushButton *mute{};
    QPushButton *fullscreen{};
    QSlider *seek{};
    QSlider *volumeSlider{};
    QComboBox *sourceSelector{};
    QComboBox *audioSelector{};
    QComboBox *subtitleSelector{};
    QTimer *progressTimer{};
    QTimer *autoHideTimer{};
    QSet<int> failedSources;
    quint64 sourceGeneration = 0;
    int sourceIndex = -1;
    double positionSeconds = 0.0;
    double durationSeconds = 0.0;
    double initialResumePosition = 0.0;
    bool seeking = false;
    bool muted = false;
    bool closing = false;
    bool subtitlePreferenceApplied = false;
    bool controlsVisible = true;
    bool loading = true;
    QRect normalGeometry;
};

} // namespace CloudStream
