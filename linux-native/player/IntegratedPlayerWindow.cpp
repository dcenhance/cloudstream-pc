#include "IntegratedPlayerWindow.h"
#include "MpvPlayerWidget.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedLayout>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace CloudStream {
namespace {
QString trackLabel(const MpvTrack &track, const QString &fallback) {
    QStringList details;
    if (!track.title.isEmpty()) details << track.title;
    if (!track.language.isEmpty() && !details.contains(track.language)) details << track.language.toUpper();
    if (!track.codec.isEmpty()) details << track.codec.toUpper();
    if (track.external) details << "External";
    return details.isEmpty() ? fallback + " " + QString::number(track.id) : details.join(" • ");
}
} // namespace

IntegratedPlayerWindow::IntegratedPlayerWindow(const SourceDiscovery &sourceDiscovery,
                                               QString title,
                                               double resumePositionSeconds,
                                               QWidget *parent,
                                               PlayerPreferences playerPreferences)
    : QDialog(parent), discovery(sourceDiscovery), preferences(playerPreferences),
      mediaTitle(std::move(title)),
      initialResumePosition(std::max(0.0, resumePositionSeconds)) {
    preferences.seekSeconds = std::clamp(preferences.seekSeconds, 5, 90);
    preferences.initialVolume = std::clamp(preferences.initialVolume, 0, 100);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(mediaTitle.isEmpty() ? "CloudStream Player" : mediaTitle);
    resize(1120, 720);
    setMinimumSize(760, 500);
    setStyleSheet(
        "QDialog{background:#000;color:#f5f4fb;}"
        "QWidget#playerChrome{background:transparent;}"
        "QWidget#playerChrome[chromeVisible=\"true\"]{background:rgba(0,0,0,72);}"
        "QWidget#playerTopBar,QWidget#playerControls{background:rgba(7,7,9,210);}"
        "QLabel#playerLoadingOverlay{background:rgba(7,7,9,215);border:1px solid rgba(255,255,255,45);border-radius:18px;padding:9px 16px;color:white;font-size:15px;font-weight:600;}"

        "QComboBox{min-height:36px;background:rgba(20,20,24,225);color:#f5f4fb;border:0;border-radius:18px;padding:0 12px;}"
        "QPushButton{min-height:36px;background:rgba(25,25,29,225);color:#f5f4fb;border:0;border-radius:18px;padding:0 14px;}"
        "QPushButton:hover{background:rgba(45,45,51,240);}"
        "QPushButton[playerAction=\"true\"]{background:transparent;border-radius:14px;padding:5px 10px;}"
        "QPushButton[playerAction=\"true\"]:hover{background:rgba(255,255,255,28);}"
        "QPushButton[playerTransport=\"true\"]{background:rgba(12,12,15,190);border-radius:28px;min-width:56px;max-width:56px;min-height:56px;max-height:56px;}"
        "QPushButton#playerPlayPause{background:rgba(12,12,15,190);border:2px solid white;border-radius:36px;min-width:72px;max-width:72px;min-height:72px;max-height:72px;}"
        "QLabel#playerSecondary{color:#b7b7c0;}"
        "QSlider::groove:horizontal{height:7px;background:#55555d;border-radius:3px;}"
        "QSlider::sub-page:horizontal{background:#536dfe;border-radius:3px;}"
        "QSlider::handle:horizontal{width:17px;margin:-5px 0;background:#f4f4f6;border-radius:8px;}"
    );

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *videoFrame = new QWidget;
    videoFrame->setStyleSheet("background:#000;");
    auto *videoStack = new QStackedLayout(videoFrame);
    videoStack->setObjectName("playerVideoStack");
    videoStack->setContentsMargins(0, 0, 0, 0);
    videoStack->setStackingMode(QStackedLayout::StackOne);
    video = new MpvPlayerWidget;
    videoStack->addWidget(video);

    chrome = new QWidget(video);
    chrome->setObjectName("playerChrome");
    chrome->setAttribute(Qt::WA_NoSystemBackground);
    chrome->setAttribute(Qt::WA_TranslucentBackground);
    chrome->setAutoFillBackground(false);
    auto *videoOverlayLayout = new QVBoxLayout(video);
    videoOverlayLayout->setContentsMargins(0, 0, 0, 0);
    videoOverlayLayout->addWidget(chrome);
    auto *chromeLayout = new QVBoxLayout(chrome);
    chromeLayout->setContentsMargins(0, 0, 0, 0);
    chromeLayout->setSpacing(0);

    topBar = new QWidget;
    topBar->setObjectName("playerTopBar");
    topBar->setVisible(preferences.showInformation);
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(20, 12, 14, 12);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titleLabel = new QLabel(mediaTitle.isEmpty() ? "CloudStream Player" : mediaTitle);
    titleLabel->setObjectName("playerTitle");
    titleLabel->setStyleSheet("font-size:17px;font-weight:700;");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    sourceStatus = new QLabel("Finding a playable source…");
    sourceStatus->setObjectName("playerSecondary");
    sourceStatus->setAlignment(Qt::AlignCenter);
    titles->addWidget(titleLabel);
    titles->addWidget(sourceStatus);
    auto *closeButton = new QPushButton;
    closeButton->setObjectName("playerBack");
    closeButton->setAccessibleName("Close player");
    closeButton->setToolTip("Back");
    closeButton->setFixedSize(42, 42);
    closeButton->setIcon(QIcon(":/icons/player-back.svg"));
    closeButton->setIconSize(QSize(26, 26));
    closeButton->setStyleSheet("padding:0;background:transparent;");
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    topLayout->addWidget(closeButton);
    topLayout->addLayout(titles, 1);
    auto *topBalance = new QWidget;
    topBalance->setFixedWidth(closeButton->width());
    topLayout->addWidget(topBalance);
    chromeLayout->addWidget(topBar);
    chromeLayout->addStretch(1);

    auto *centerLayout = new QHBoxLayout;
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(24);
    centerLayout->addStretch();
    rewind = new QPushButton(QString::number(preferences.seekSeconds));
    rewind->setObjectName("playerRewind");
    rewind->setProperty("playerTransport", true);
    rewind->setToolTip("Back " + QString::number(preferences.seekSeconds) + " seconds");
    rewind->setAccessibleName(rewind->toolTip());
    rewind->setIcon(QIcon(":/icons/player-rewind.svg"));
    rewind->setIconSize(QSize(34, 34));
    playPause = new QPushButton;
    playPause->setObjectName("playerPlayPause");
    playPause->setIcon(QIcon(":/icons/pause.svg"));
    playPause->setIconSize(QSize(30, 30));
    playPause->setToolTip("Pause");
    playPause->setAccessibleName("Play or pause");
    forward = new QPushButton(QString::number(preferences.seekSeconds));
    forward->setObjectName("playerForward");
    forward->setProperty("playerTransport", true);
    forward->setToolTip("Forward " + QString::number(preferences.seekSeconds) + " seconds");
    forward->setAccessibleName(forward->toolTip());
    forward->setIcon(QIcon(":/icons/player-forward.svg"));
    forward->setIconSize(QSize(34, 34));
    centerLayout->addWidget(rewind);
    centerLayout->addWidget(playPause);
    centerLayout->addWidget(forward);
    centerLayout->addStretch();
    chromeLayout->addLayout(centerLayout);
    loadingLabel = new QLabel("Loading stream…");
    loadingLabel->setObjectName("playerLoadingOverlay");
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    loadingLabel->hide();
    chromeLayout->addWidget(loadingLabel, 0, Qt::AlignHCenter);
    chromeLayout->addStretch(1);

    controls = new QWidget;
    controls->setObjectName("playerControls");
    auto *controlsLayout = new QVBoxLayout(controls);
    controlsLayout->setContentsMargins(18, 10, 18, 14);
    controlsLayout->setSpacing(8);

    auto *timeline = new QHBoxLayout;
    timeline->setSpacing(10);
    timeLabel = new QLabel("0:00 / 0:00");
    timeLabel->setObjectName("playerPosition");
    timeLabel->setText("0:00");
    timeLabel->setMinimumWidth(54);
    timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    timeline->addWidget(timeLabel);
    seek = new QSlider(Qt::Horizontal);
    seek->setRange(0, 1000);
    seek->setAccessibleName("Playback position");
    timeline->addWidget(seek, 1);
    durationLabel = new QLabel("0:00");
    durationLabel->setObjectName("playerDuration");
    durationLabel->setMinimumWidth(54);
    timeline->addWidget(durationLabel);
    controlsLayout->addLayout(timeline);

    sourceSelector = new QComboBox(controls);
    sourceSelector->setObjectName("sourceSelector");
    sourceSelector->setAccessibleName("Video source and quality");
    sourceSelector->setMinimumWidth(230);
    for (const auto &source : discovery.sources) sourceSelector->addItem(source.displayLabel());
    sourceSelector->hide();

    audioSelector = new QComboBox(controls);
    audioSelector->setObjectName("audioTrackSelector");
    audioSelector->setAccessibleName("Audio track");
    audioSelector->addItem("Audio: Auto", -1);
    audioSelector->hide();

    subtitleSelector = new QComboBox(controls);
    subtitleSelector->setObjectName("subtitleTrackSelector");
    subtitleSelector->setAccessibleName("Subtitles");
    subtitleSelector->addItem("Subtitles: Off", -1);
    subtitleSelector->hide();

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    auto *lockControl = new QPushButton("Lock");
    lockControl->setObjectName("playerLock");
    lockControl->setAccessibleName("Lock player controls");
    lockControl->setIcon(QIcon(":/icons/player-lock.svg"));
    auto *scaleControl = new QPushButton("Fit");
    scaleControl->setObjectName("playerScale");
    scaleControl->setAccessibleName("Change video scaling");
    scaleControl->setIcon(QIcon(":/icons/player-aspect.svg"));
    auto *speedControl = new QPushButton("1×");
    speedControl->setObjectName("playerSpeed");
    speedControl->setAccessibleName("Change playback speed");
    speedControl->setIcon(QIcon(":/icons/speed.svg"));
    auto *sourcesControl = new QPushButton("Sources");
    sourcesControl->setObjectName("playerSources");
    sourcesControl->setAccessibleName("Choose playback source");
    sourcesControl->setIcon(QIcon(":/icons/source-selector.svg"));
    auto *tracksControl = new QPushButton("Tracks");
    tracksControl->setObjectName("playerTracks");
    tracksControl->setAccessibleName("Choose audio and subtitle tracks");
    tracksControl->setIcon(QIcon(":/icons/player-tracks.svg"));
    for (auto *action : {lockControl, scaleControl, speedControl, sourcesControl, tracksControl}) {
        action->setProperty("playerAction", true);
        action->setIconSize(QSize(22, 22));
    }
    buttonRow->addWidget(lockControl);
    buttonRow->addWidget(scaleControl);
    buttonRow->addWidget(speedControl);
    buttonRow->addWidget(sourcesControl);
    buttonRow->addWidget(tracksControl);
    buttonRow->addStretch();
    mute = new QPushButton("Sound");
    mute->setObjectName("playerMute");
    mute->setAccessibleName("Mute or unmute");
    mute->setProperty("playerAction", true);
    mute->setIcon(QIcon(":/icons/volume-up.svg"));
    mute->setIconSize(QSize(22, 22));
    volumeSlider = new QSlider(Qt::Horizontal);
    volumeSlider->setObjectName("playerVolume");
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(preferences.initialVolume);
    volumeSlider->setMaximumWidth(110);
    volumeSlider->setAccessibleName("Volume");
    fullscreen = new QPushButton("Full screen");
    fullscreen->setObjectName("playerFullscreen");
    fullscreen->setAccessibleName("Toggle full screen");
    fullscreen->setProperty("playerAction", true);
    fullscreen->setIcon(QIcon(":/icons/player-fullscreen.svg"));
    fullscreen->setIconSize(QSize(22, 22));
    buttonRow->addWidget(mute);
    buttonRow->addWidget(volumeSlider);
    buttonRow->addWidget(fullscreen);
    controlsLayout->addLayout(buttonRow);
    chromeLayout->addWidget(controls);

    videoStack->setCurrentWidget(video);
    root->addWidget(videoFrame, 1);

    connect(video, &MpvPlayerWidget::positionChanged, this, [this](double value) {
        positionSeconds = value;
        if (!seeking && durationSeconds > 0.0) seek->setValue(
            std::clamp(static_cast<int>((positionSeconds / durationSeconds) * 1000.0), 0, 1000));
        refreshTimeLabel();
    });
    connect(video, &MpvPlayerWidget::durationChanged, this, [this](double value) {
        durationSeconds = value;
        refreshTimeLabel();
    });
    connect(video, &MpvPlayerWidget::pausedChanged, this, [this](bool paused) {
        playPause->setIcon(QIcon(paused ? ":/icons/play.svg" : ":/icons/pause.svg"));
        playPause->setToolTip(paused ? "Play" : "Pause");
        if (paused) {
            autoHideTimer->stop();
            setControlsVisible(true);
        } else {
            scheduleAutoHide();
        }
    });
    connect(video, &MpvPlayerWidget::mutedChanged, this, [this](bool value) {
        muted = value;
        mute->setText(value ? "Muted" : "Sound");
        mute->setIcon(QIcon(value ? ":/icons/player-muted.svg" : ":/icons/volume-up.svg"));
    });
    connect(video, &MpvPlayerWidget::loadingChanged, this,
            [this](bool loading) {
        loadingLabel->setVisible(loading);
        this->loading = loading;
        if (loading) {
            autoHideTimer->stop();
            setControlsVisible(true);
        } else {
            scheduleAutoHide();
        }
    });
    connect(video, &MpvPlayerWidget::fileLoaded, this, [this] {
        if (sourceIndex >= 0 && sourceIndex < discovery.sources.size()) {
            sourceStatus->setText("Playing " + discovery.sources[sourceIndex].displayLabel() +
                                  " • " + QString::number(discovery.sources.size()) + " source(s) found");
        }
    });
    connect(video, &MpvPlayerWidget::tracksChanged, this, [this](const QList<MpvTrack> &tracks) {
        int selectedAudio = -1;
        int selectedSubtitle = -1;
        for (const auto &track : tracks) {
            if (track.selected && track.type == "audio") selectedAudio = track.id;
            if (track.selected && track.type == "sub") selectedSubtitle = track.id;
        }
        const QSignalBlocker audioBlocker(audioSelector);
        const QSignalBlocker subtitleBlocker(subtitleSelector);
        audioSelector->clear();
        audioSelector->addItem("Audio: Auto", -1);
        subtitleSelector->clear();
        subtitleSelector->addItem("Subtitles: Off", -1);
        int selectedAudioIndex = 0;
        int selectedSubtitleIndex = 0;
        for (const auto &track : tracks) {
            if (track.type == "audio") {
                audioSelector->addItem("Audio: " + trackLabel(track, "Track"), track.id);
                if (track.id == selectedAudio) selectedAudioIndex = audioSelector->count() - 1;
            } else if (track.type == "sub") {
                subtitleSelector->addItem("Subtitles: " + trackLabel(track, "Track"), track.id);
                if (track.id == selectedSubtitle) selectedSubtitleIndex = subtitleSelector->count() - 1;
            }
        }
        audioSelector->setCurrentIndex(selectedAudioIndex);
        subtitleSelector->setCurrentIndex(selectedSubtitleIndex);
        if (preferences.selectFirstSubtitle && !subtitlePreferenceApplied && selectedSubtitle < 0) {
            for (int index = 1; index < subtitleSelector->count(); ++index) {
                const auto id = subtitleSelector->itemData(index).toInt();
                if (id < 0) continue;
                subtitleSelector->setCurrentIndex(index);
                video->setSubtitleTrack(id);
                subtitlePreferenceApplied = true;
                break;
            }
        }
    });
    connect(video, &MpvPlayerWidget::playbackError, this, &IntegratedPlayerWindow::handlePlaybackError);
    connect(video, &MpvPlayerWidget::endReached, this, [this] {
        sourceStatus->setText("Playback finished");
        video->setPaused(true);
    });
    connect(playPause, &QPushButton::clicked, video, &MpvPlayerWidget::togglePaused);
    connect(rewind, &QPushButton::clicked, this, [this] { video->seekBy(-preferences.seekSeconds); });
    connect(forward, &QPushButton::clicked, this, [this] { video->seekBy(preferences.seekSeconds); });
    connect(seek, &QSlider::sliderPressed, this, [this] { seeking = true; });
    connect(seek, &QSlider::sliderMoved, this, [this](int value) {
        const auto duration = durationSeconds > 0.0 ? durationSeconds : video->duration();
        if (duration > 0.0) video->seekTo((value / 1000.0) * duration);
    });
    connect(seek, &QSlider::sliderReleased, this, [this] {
        seeking = false;
        const auto duration = durationSeconds > 0.0 ? durationSeconds : video->duration();
        if (duration > 0.0) video->seekTo((seek->value() / 1000.0) * duration);
    });
    connect(volumeSlider, &QSlider::valueChanged, video, &MpvPlayerWidget::setVolume);
    video->setVolume(preferences.initialVolume);
    connect(mute, &QPushButton::clicked, this, [this] {
        video->setMuted(!video->isMuted());
    });
    connect(fullscreen, &QPushButton::clicked, this, &IntegratedPlayerWindow::toggleFullscreen);
    connect(scaleControl, &QPushButton::clicked, this, [this, scaleControl] {
        const bool fill = !video->fillMode();
        video->setFillMode(fill);
        scaleControl->setText(fill ? "Fill" : "Fit");
        scaleControl->setToolTip(fill ? "Crop video to fill the window" : "Fit the whole video in the window");
    });
    connect(speedControl, &QPushButton::clicked, this, &IntegratedPlayerWindow::showSpeedDialog);
    connect(sourcesControl, &QPushButton::clicked, this, &IntegratedPlayerWindow::showSourceDialog);
    connect(tracksControl, &QPushButton::clicked, this, &IntegratedPlayerWindow::showTrackDialog);
    connect(lockControl, &QPushButton::clicked, this,
            [this, closeButton, lockControl] {
        const bool locked = !lockControl->property("locked").toBool();
        lockControl->setProperty("locked", locked);
        lockControl->setText(locked ? "Unlock controls" : "Lock");
        rewind->setEnabled(!locked);
        playPause->setEnabled(!locked);
        forward->setEnabled(!locked);
        topBar->setEnabled(!locked);
        closeButton->setEnabled(!locked);
        const auto children = controls->findChildren<QWidget *>();
        for (auto *child : children) {
            if (child != lockControl && !lockControl->isAncestorOf(child)) child->setEnabled(!locked);
        }
        lockControl->setEnabled(true);
    });
    connect(sourceSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index == sourceIndex) return;
        failedSources.clear();
        switchSource(index, false);
    });
    connect(audioSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        video->setAudioTrack(audioSelector->itemData(index).toInt());
    });
    connect(subtitleSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        video->setSubtitleTrack(subtitleSelector->itemData(index).toInt());
    });

    auto shortcut = [this](const QKeySequence &sequence, const std::function<void()> &action) {
        auto *value = new QShortcut(sequence, this);
        connect(value, &QShortcut::activated, this, action);
    };
    shortcut(Qt::Key_Space, [this] { video->togglePaused(); });
    shortcut(Qt::Key_Left, [this] { video->seekBy(-preferences.seekSeconds); });
    shortcut(Qt::Key_Right, [this] { video->seekBy(preferences.seekSeconds); });
    shortcut(Qt::Key_F, [this] { toggleFullscreen(); });
    shortcut(Qt::Key_M, [this] { mute->click(); });
    shortcut(Qt::Key_S, [this] { showSourceDialog(); });
    shortcut(Qt::Key_T, [this] { showTrackDialog(); });
    shortcut(Qt::Key_E, [this] { showSpeedDialog(); });
    shortcut(Qt::Key_R, [scaleControl] { scaleControl->click(); });
    shortcut(Qt::Key_H, [this] {
        setControlsVisible(!controlsVisible);
        scheduleAutoHide();
    });
    shortcut(Qt::Key_Escape, [this] {
        if (isFullScreen()) toggleFullscreen();
        else close();
    });

    progressTimer = new QTimer(this);
    progressTimer->setInterval(1000);
    connect(progressTimer, &QTimer::timeout, this, [this] {
        if (durationSeconds > 0.0) emit progressUpdated(positionSeconds, durationSeconds);
    });
    progressTimer->start();

    autoHideTimer = new QTimer(this);
    autoHideTimer->setSingleShot(true);
    connect(autoHideTimer, &QTimer::timeout, this, [this] {
        if (!video->isPaused() && !loading) setControlsVisible(false);
    });
    chrome->setProperty("chromeVisible", true);
    chrome->setMouseTracking(true);
    chrome->installEventFilter(this);
    video->setMouseTracking(true);
    video->installEventFilter(this);

    if (!video->isAvailable()) {
        loadingLabel->setText(video->initializationError());
        sourceStatus->setText("Integrated player is unavailable");
    } else if (discovery.sources.isEmpty()) {
        loadingLabel->setText("No playable HLS, DASH, or direct-video source was found.");
        sourceStatus->setText("No playable source");
    } else {
        QTimer::singleShot(0, this, [this] { switchSource(0, false); });
    }
}

double IntegratedPlayerWindow::position() const { return positionSeconds; }
double IntegratedPlayerWindow::duration() const { return durationSeconds; }
int IntegratedPlayerWindow::currentSourceIndex() const { return sourceIndex; }

void IntegratedPlayerWindow::closeEvent(QCloseEvent *event) {
    closing = true;
    if (durationSeconds > 0.0) emit progressUpdated(positionSeconds, durationSeconds);
    QDialog::closeEvent(event);
}

bool IntegratedPlayerWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == chrome || watched == video) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            toggleFullscreen();
            setControlsVisible(true);
            scheduleAutoHide();
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                if (controlsVisible && !video->isPaused() && !loading) {
                    setControlsVisible(false);
                } else {
                    setControlsVisible(true);
                    scheduleAutoHide();
                }
                return true;
            }
        }
        if (event->type() == QEvent::MouseMove) {
            if (!controlsVisible) setControlsVisible(true);
            scheduleAutoHide();
        }
    }
    return QDialog::eventFilter(watched, event);
}

QString IntegratedPlayerWindow::formatTime(double seconds) {
    const auto total = std::max(0, static_cast<int>(std::round(seconds)));
    const auto hours = total / 3600;
    const auto minutes = (total % 3600) / 60;
    const auto remaining = total % 60;
    if (hours > 0) return QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(remaining, 2, 10, QChar('0'));
    return QString("%1:%2").arg(minutes).arg(remaining, 2, 10, QChar('0'));
}

void IntegratedPlayerWindow::showSourceDialog() {
    auto *dialog = new QDialog(this);
    dialog->setObjectName("playerSourceDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Sources");
    dialog->resize(680, 470);
    dialog->setMinimumSize(520, 380);
    dialog->setStyleSheet(
        "QDialog{background:#0b0b0e;color:#f5f4f8;}"
        "QLabel#dialogTitle{font-size:22px;font-weight:700;}"
        "QLabel#dialogDetail{color:#b8bac4;background:#15161b;border-radius:8px;padding:10px;}"
        "QListWidget{background:#0b0b0e;border:0;outline:0;font-size:15px;}"
        "QListWidget::item{min-height:52px;padding:0 12px;border-radius:5px;}"
        "QListWidget::item:selected{background:#24293c;color:white;}"
        "QPushButton{min-height:38px;padding:0 18px;background:#222329;color:white;border:0;border-radius:6px;}"
        "QPushButton[primary=\"true\"]{background:#536dfe;color:white;font-weight:700;}"
    );
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);
    auto *title = new QLabel("Sources");
    title->setObjectName("dialogTitle");
    layout->addWidget(title);
    auto *list = new QListWidget;
    list->setObjectName("playerSourceList");
    for (int index = 0; index < discovery.sources.size(); ++index) {
        const auto &source = discovery.sources[index];
        auto *item = new QListWidgetItem((index == sourceIndex ? "✓  " : "    ") +
                                         source.displayLabel());
        item->setData(Qt::UserRole, index);
        item->setData(Qt::UserRole + 1, source.displayLabel());
        list->addItem(item);
    }
    if (sourceIndex >= 0 && sourceIndex < list->count()) list->setCurrentRow(sourceIndex);
    else if (list->count() > 0) list->setCurrentRow(0);
    layout->addWidget(list, 1);
    auto *detail = new QLabel;
    detail->setObjectName("dialogDetail");
    detail->setWordWrap(true);
    layout->addWidget(detail);
    const auto update = [this, list, detail](int row) {
        if (row < 0 || row >= discovery.sources.size()) {
            detail->clear();
            return;
        }
        for (int index = 0; index < list->count(); ++index) {
            auto *item = list->item(index);
            item->setText((index == row ? "✓  " : "    ") +
                          item->data(Qt::UserRole + 1).toString());
        }
        const auto &source = discovery.sources[row];
        QStringList facts{source.hosterName(), source.type.isEmpty() ? QString("VIDEO") : source.type};
        if (source.quality > 0) facts << QString::number(source.quality) + "p";
        if (!source.referer.isEmpty()) facts << "Referer preserved";
        if (!source.headers.isEmpty()) facts << QString::number(source.headers.size()) + " header(s)";
        detail->setText(facts.join(" • ") +
                        (row == sourceIndex ? "\nCurrently playing — Apply retries this source." :
                                              "\nApply switches immediately and keeps your position."));
    };
    connect(list, &QListWidget::currentRowChanged, dialog, update);
    update(list->currentRow());
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Apply");
    buttons->button(QDialogButtonBox::Ok)->setProperty("primary", true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, list] {
        const auto row = list->currentRow();
        if (row >= 0) switchSource(row, false);
        dialog->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    const bool resumePlayback = !video->isPaused();
    if (resumePlayback) video->setPaused(true);
    autoHideTimer->stop();
    connect(dialog, &QDialog::finished, this, [this, resumePlayback] {
        if (resumePlayback && !closing) video->setPaused(false);
        scheduleAutoHide();
    });
    dialog->open();
}

void IntegratedPlayerWindow::showTrackDialog() {
    auto *dialog = new QDialog(this);
    dialog->setObjectName("playerTrackDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Tracks");
    dialog->resize(800, 480);
    dialog->setMinimumSize(620, 390);
    dialog->setStyleSheet(
        "QDialog{background:#0b0b0e;color:#f5f4f8;}"
        "QLabel[heading=\"true\"]{font-size:20px;font-weight:700;padding:6px 10px;}"
        "QListWidget{background:#0b0b0e;border:0;outline:0;font-size:15px;}"
        "QListWidget::item{min-height:48px;padding:0 12px;border-radius:5px;}"
        "QListWidget::item:selected{background:#24293c;color:white;}"
        "QPushButton{min-height:38px;padding:0 18px;background:#222329;color:white;border:0;border-radius:6px;}"
        "QPushButton[primary=\"true\"]{background:#536dfe;color:white;font-weight:700;}"
    );
    auto *root = new QVBoxLayout(dialog);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(8);
    auto *columns = new QHBoxLayout;
    columns->setSpacing(22);
    auto *audioColumn = new QVBoxLayout;
    auto *audioHeading = new QLabel("Audio tracks");
    audioHeading->setProperty("heading", true);
    audioColumn->addWidget(audioHeading);
    auto *audioList = new QListWidget;
    audioList->setObjectName("playerAudioTracks");
    auto *audioAuto = new QListWidgetItem("Auto");
    audioAuto->setData(Qt::UserRole, -1);
    audioList->addItem(audioAuto);
    auto *subtitleColumn = new QVBoxLayout;
    auto *subtitleHeading = new QLabel("Subtitles");
    subtitleHeading->setProperty("heading", true);
    subtitleColumn->addWidget(subtitleHeading);
    auto *subtitleList = new QListWidget;
    subtitleList->setObjectName("playerSubtitleTracks");
    auto *subtitleOff = new QListWidgetItem("Off");
    subtitleOff->setData(Qt::UserRole, -1);
    subtitleList->addItem(subtitleOff);
    int selectedAudioRow = 0;
    int selectedSubtitleRow = 0;
    for (const auto &track : video->tracks()) {
        if (track.type == "audio") {
            auto *item = new QListWidgetItem(trackLabel(track, "Audio"));
            item->setData(Qt::UserRole, track.id);
            audioList->addItem(item);
            if (track.selected) selectedAudioRow = audioList->count() - 1;
        } else if (track.type == "sub") {
            auto *item = new QListWidgetItem(trackLabel(track, "Subtitle"));
            item->setData(Qt::UserRole, track.id);
            subtitleList->addItem(item);
            if (track.selected) selectedSubtitleRow = subtitleList->count() - 1;
        }
    }
    audioList->setCurrentRow(selectedAudioRow);
    subtitleList->setCurrentRow(selectedSubtitleRow);
    audioColumn->addWidget(audioList, 1);
    subtitleColumn->addWidget(subtitleList, 1);
    columns->addLayout(audioColumn, 1);
    columns->addLayout(subtitleColumn, 1);
    root->addLayout(columns, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Apply");
    buttons->button(QDialogButtonBox::Ok)->setProperty("primary", true);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dialog,
            [this, dialog, audioList, subtitleList] {
        if (auto *item = audioList->currentItem()) video->setAudioTrack(item->data(Qt::UserRole).toInt());
        if (auto *item = subtitleList->currentItem()) video->setSubtitleTrack(item->data(Qt::UserRole).toInt());
        dialog->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    const bool resumePlayback = !video->isPaused();
    if (resumePlayback) video->setPaused(true);
    autoHideTimer->stop();
    connect(dialog, &QDialog::finished, this, [this, resumePlayback] {
        if (resumePlayback && !closing) video->setPaused(false);
        scheduleAutoHide();
    });
    dialog->open();
}

void IntegratedPlayerWindow::showSpeedDialog() {
    auto *dialog = new QDialog(this);
    dialog->setObjectName("playerSpeedDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Playback speed");
    dialog->resize(520, 250);
    dialog->setStyleSheet(
        "QDialog{background:#0b0b0e;color:#f5f4f8;}"
        "QLabel#speedValue{font-size:28px;font-weight:700;}"
        "QPushButton{min-height:38px;padding:0 14px;background:#222329;color:white;border:0;border-radius:6px;}"
        "QPushButton[primary=\"true\"]{background:#536dfe;color:white;font-weight:700;}"
        "QSlider::groove:horizontal{height:5px;background:#55555d;border-radius:2px;}"
        "QSlider::sub-page:horizontal{background:#536dfe;border-radius:2px;}"
        "QSlider::handle:horizontal{width:17px;margin:-6px 0;background:white;border-radius:8px;}"
    );
    auto *root = new QVBoxLayout(dialog);
    root->setContentsMargins(22, 18, 22, 18);
    auto *value = new QLabel;
    value->setObjectName("speedValue");
    value->setAlignment(Qt::AlignCenter);
    root->addWidget(value);
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setObjectName("playerSpeedSlider");
    slider->setRange(25, 200);
    slider->setValue(static_cast<int>(std::round(video->playbackSpeed() * 100.0)));
    root->addWidget(slider);
    auto *presets = new QHBoxLayout;
    for (const int percentage : {50, 75, 100, 125, 150, 200}) {
        auto *preset = new QPushButton(QString::number(percentage / 100.0, 'g', 3) + "×");
        preset->setObjectName("playerSpeed" + QString::number(percentage));
        connect(preset, &QPushButton::clicked, slider, [slider, percentage] { slider->setValue(percentage); });
        presets->addWidget(preset);
    }
    root->addLayout(presets);
    const auto originalSpeed = video->playbackSpeed();
    const auto applySpeed = [this, value](int percentage) {
        const auto speed = percentage / 100.0;
        value->setText(QString::number(speed, 'g', 3) + "×");
        video->setPlaybackSpeed(speed);
        if (auto *button = findChild<QPushButton *>("playerSpeed"))
            button->setText(QString::number(speed, 'g', 3) + "×");
    };
    connect(slider, &QSlider::valueChanged, dialog, applySpeed);
    applySpeed(slider->value());
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Apply");
    buttons->button(QDialogButtonBox::Ok)->setProperty("primary", true);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, [this, dialog, originalSpeed] {
        video->setPlaybackSpeed(originalSpeed);
        if (auto *button = findChild<QPushButton *>("playerSpeed"))
            button->setText(QString::number(originalSpeed, 'g', 3) + "×");
        dialog->reject();
    });
    autoHideTimer->stop();
    connect(dialog, &QDialog::finished, this, [this] { scheduleAutoHide(); });
    dialog->open();
}

void IntegratedPlayerWindow::switchSource(int index, bool automaticFallback) {
    if (index < 0 || index >= discovery.sources.size()) return;
    ++sourceGeneration;
    sourceIndex = index;
    if (!automaticFallback) failedSources.clear();
    const QSignalBlocker blocker(sourceSelector);
    sourceSelector->setCurrentIndex(index);
    loadingLabel->setText(automaticFallback ? "Trying the next source…" : "Loading stream…");
    loadingLabel->show();
    sourceStatus->setText((automaticFallback ? "Fallback: " : "Loading ") + discovery.sources[index].displayLabel());
    const auto resume = positionSeconds > 0.0 ? positionSeconds : initialResumePosition;
    initialResumePosition = 0.0;
    video->loadSource(discovery.sources[index], discovery.subtitles, resume);
}

void IntegratedPlayerWindow::handlePlaybackError(const QString &message) {
    if (closing || sourceIndex < 0) return;
    failedSources.insert(sourceIndex);
    if (!preferences.automaticFallback) {
        loadingLabel->setText("Playback failed. Choose another source to retry.");
        loadingLabel->show();
        sourceStatus->setText("Playback failed: " + message);
        return;
    }
    for (int offset = 1; offset <= discovery.sources.size(); ++offset) {
        const auto candidate = (sourceIndex + offset) % discovery.sources.size();
        if (failedSources.contains(candidate)) continue;
        sourceStatus->setText(discovery.sources[sourceIndex].hosterName() + " failed: " + message +
                              " • trying " + discovery.sources[candidate].displayLabel());
        const auto expectedGeneration = sourceGeneration;
        QTimer::singleShot(600, this, [this, candidate, expectedGeneration] {
            if (sourceGeneration == expectedGeneration) switchSource(candidate, true);
        });
        return;
    }
    loadingLabel->setText("Every discovered source failed. Choose a source to retry.");
    loadingLabel->show();
    sourceStatus->setText("Playback failed: " + message);
}

void IntegratedPlayerWindow::toggleFullscreen() {
    if (isFullScreen()) {
        showNormal();
        if (normalGeometry.isValid()) setGeometry(normalGeometry);
        fullscreen->setText("Full screen");
        fullscreen->setIcon(QIcon(":/icons/player-fullscreen.svg"));
    } else {
        normalGeometry = geometry();
        showFullScreen();
        fullscreen->setText("Exit full screen");
        fullscreen->setIcon(QIcon(":/icons/player-fullscreen-exit.svg"));
    }
}

void IntegratedPlayerWindow::refreshTimeLabel() {
    timeLabel->setText(formatTime(positionSeconds));
    durationLabel->setText(formatTime(durationSeconds));
}

void IntegratedPlayerWindow::setControlsVisible(bool visible) {
    controlsVisible = visible;
    topBar->setVisible(visible && preferences.showInformation);
    rewind->setVisible(visible);
    playPause->setVisible(visible);
    forward->setVisible(visible);
    controls->setVisible(visible);
    chrome->setProperty("chromeVisible", visible);
    chrome->style()->unpolish(chrome);
    chrome->style()->polish(chrome);
    chrome->setCursor(visible ? Qt::ArrowCursor : Qt::BlankCursor);
}

void IntegratedPlayerWindow::scheduleAutoHide() {
    if (!autoHideTimer || preferences.autoHideDelayMs <= 0 || video->isPaused() || loading) return;
    autoHideTimer->start(std::max(50, preferences.autoHideDelayMs));
}

} // namespace CloudStream
