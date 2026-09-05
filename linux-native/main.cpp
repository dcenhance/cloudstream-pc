#include "app/Logger.h"
#include "app/ProcessCompletion.h"
#include "app/PackagedRuntimeEnvironment.h"
#include "app/ProviderHostCommand.h"
#include "details/DetailsPresentation.h"
#include "downloads/DownloadManager.h"
#include "downloads/DownloadQueueStore.h"
#include "episodes/EpisodeCatalog.h"
#include "extensions/ExtensionRegistry.h"
#include "extensions/ExtensionListFilter.h"
#include "extensions/ExtensionInstallBatch.h"
#include "history/WatchHistoryStore.h"
#include "input/GamepadNavigation.h"
#include "network/CloudStreamRequest.h"
#include "media/ArtworkLoader.h"
#include "media/ArtworkSizing.h"
#include "providers/HomeContentLimiter.h"
#include "providers/HomeProcessResult.h"
#include "providers/HomeHeroSelection.h"
#include "providers/ProviderConfiguration.h"
#include "providers/ProviderDiscoveryGeneration.h"
#include "providers/ProviderPickerDialog.h"
#include "providers/ProviderPreferenceFilter.h"
#include "providers/ProviderSelectionModel.h"
#include "providers/ProviderValidation.h"
#include "player/IntegratedPlayerWindow.h"
#include "player/MpvIpcProtocol.h"
#include "player/PlayerCommand.h"
#include "player/SourceCatalog.h"
#include "repositories/RepositoryManifestParser.h"
#include "repositories/RepositoryUrlResolver.h"
#include "search/SearchHistoryModel.h"
#include "settings/SettingsPane.h"
#include "storage/XdgPaths.h"
#include "ui/SmoothScrollController.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLabel>
#include <QStandardPaths>
#include <QLineEdit>
#include <QListWidget>
#include <QListView>
#include <QLocalSocket>
#include <QLocale>
#include <QMainWindow>
#include <QMenu>
#include <QLinearGradient>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointer>
#include <QPersistentModelIndex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSettings>
#include <QShortcut>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSplashScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStorageInfo>
#include <QStyle>
#include <QStyleFactory>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>

namespace {
const QString purple = "#536dfe";
const QString bg = "#040405";
const QString surface = "#101012";
const QString surface2 = "#18181b";
const QString text = "#f4f3fa";
const QString muted = "#a6a6b5";
QString baseApplicationStyleSheet;
QString defaultApplicationStyleSheet;
const QString converterId = "dex2jar-2.4.38-preserve-names-frames-v2";

QString appearanceOverrides(const QSettings &settings) {
    const auto theme = settings.value("interface/theme", "Android dark").toString();
    QString background = "#040405";
    QString surface = "#101012";
    QString raised = "#18181b";
    QString foreground = "#f4f3fa";
    QString accentColor = "#536dfe";
    if (theme == "AMOLED black") {
        background = "#000000";
        surface = "#050506";
        raised = "#0d0d10";
        accentColor = "#7186ff";
    } else if (theme == "Graphite") {
        background = "#17191d";
        surface = "#22252a";
        raised = "#2c3037";
        foreground = "#f1f3f5";
        accentColor = "#7893ff";
    } else if (theme == "High contrast") {
        background = "#000000";
        surface = "#111111";
        raised = "#202020";
        foreground = "#ffffff";
        accentColor = "#9aaaff";
    }
    const auto density = settings.value("interface/density", "Comfortable").toString();
    const int fontSize = density == "Compact" ? 13 : density == "Spacious" ? 15 : density == "TV / 10-foot" ? 17 : 14;
    const int horizontalPadding = density == "Compact" ? 10 : density == "TV / 10-foot" ? 20 : 16;
    return QString(R"(
        QWidget { color:%2; font-size:%3px; }
        QScrollArea, QScrollArea > QWidget > QWidget { background:%1; }
        #sidebar { background:%1; }
        #card, #homeEmptyState, #detailsHero, #storagePanel, #selectionBar { background:%4; border:0; border-radius:18px; }
        QPushButton { background:%4; color:%2; border:0; border-radius:8px; padding:0 %5px; }
        QPushButton[nav="true"] { border-radius:23px; padding:0 %5px; }
        QPushButton[nav="true"][expanded="true"] { border-radius:12px; text-align:left; padding-left:16px; }
        QLineEdit, QComboBox { background:%4; color:%2; border:0; border-radius:20px; }
        QListWidget { color:%2; border:0; border-radius:0; }
        QTabBar::tab { color:%2; border-radius:18px; }
        QTabBar::tab:selected, QPushButton[chip="true"]:checked { background:%6; border-radius:18px; }
        QProgressBar::chunk { background:%6; }
    )").arg(background, foreground).arg(fontSize).arg(raised).arg(horizontalPadding).arg(accentColor);
}

QNetworkRequest cloudStreamRequest(const QUrl &url) {
    return CloudStream::CloudStreamRequest::metadata(url);
}

QPushButton *button(const QString &label, bool primary = false) {
    auto *b = new QPushButton(label);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(42);
    b->setProperty("primary", primary);
    return b;
}

QLabel *title(const QString &value, int size = 24) {
    auto *label = new QLabel(value);
    label->setStyleSheet(QString("font-size:%1px;font-weight:700;color:%2;").arg(size).arg(text));
    return label;
}

class HomeHeroBanner final : public QWidget {
public:
    explicit HomeHeroBanner(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground, true);
    }

    void setBackdrop(const QPixmap &value) {
        backdrop = value;
        rebuildSurface();
        update();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        rebuildSurface();
    }

    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        if (!surface.isNull()) painter.drawPixmap(0, 0, surface);
    }

private:
    void rebuildSurface() {
        if (width() <= 0 || height() <= 0) return;
        const auto ratio = devicePixelRatioF();
        QPixmap next(QSize(qCeil(width() * ratio), qCeil(height() * ratio)));
        next.setDevicePixelRatio(ratio);
        next.fill(Qt::transparent);
        QPainter painter(&next);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(rect(), 18, 18);
        painter.setClipPath(clip);
        painter.fillRect(rect(), QColor("#11131a"));
        if (!backdrop.isNull()) {
            const bool portrait = backdrop.height() > backdrop.width();
            const auto scaled = backdrop.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const QRect source((scaled.width() - width()) / 2, (scaled.height() - height()) / 2,
                               width(), height());
            painter.setOpacity(portrait ? 0.28 : 1.0);
            painter.drawPixmap(rect(), scaled, source);
            painter.setOpacity(1.0);
            if (portrait) {
                const auto poster = backdrop.scaledToHeight(height(), Qt::SmoothTransformation);
                painter.drawPixmap(width() - poster.width(), 0, poster);
            }
        }
        QLinearGradient horizontal(0, 0, width(), 0);
        horizontal.setColorAt(0.0, QColor(4, 5, 9, 245));
        horizontal.setColorAt(0.48, QColor(4, 5, 9, 155));
        horizontal.setColorAt(1.0, QColor(4, 5, 9, 35));
        painter.fillRect(rect(), horizontal);
        QLinearGradient vertical(0, 0, 0, height());
        vertical.setColorAt(0.0, QColor(0, 0, 0, 15));
        vertical.setColorAt(0.62, QColor(0, 0, 0, 30));
        vertical.setColorAt(1.0, QColor(0, 0, 0, 210));
        painter.fillRect(rect(), vertical);
        painter.end();
        surface = std::move(next);
    }

    QPixmap backdrop;
    QPixmap surface;
};


class CloudStreamWindow final : public QMainWindow {
public:
    explicit CloudStreamWindow(bool initializeContent = true)
        : settings("CloudStream", "CloudStream Linux"),
          extensionRegistry(CloudStream::XdgPaths::dataDir() + "/extension-registry.json"),
          history(CloudStream::XdgPaths::dataDir() + "/watch-history.json"),
          downloadQueue(CloudStream::XdgPaths::dataDir() + "/download-queue.json"),
          downloadManager(&downloadQueue) {
        setWindowTitle(
#ifdef Q_OS_WIN
            "CloudStream Windows"
#else
            "CloudStream Linux"
#endif
        );
        resize(1180, 760);
        setMinimumSize(900, 600);
        artworkLoader = new CloudStream::ArtworkLoader(
            CloudStream::XdgPaths::cacheDir() + "/images", this);
        migrateExtensionState();
        build();
        applyAppearance();
        setupGamepadNavigation();
        CloudStream::SmoothScrollController::attachRecursively(this);
        connect(&downloadManager, &CloudStream::DownloadManager::queueChanged, this, [this] {
            if (downloadsRefresh) downloadsRefresh();
        });
        connect(&downloadManager, &CloudStream::DownloadManager::message, this, [this](const QString &message) {
            if (status) status->setText(message);
        });
        if (initializeContent) {
            startStartupAnimation();
            QTimer::singleShot(0, this, [this] {
                loadExtensions();
                if (!convertDownloadedAndroidExtensions()) refreshProviderChoices();
                downloadManager.start();
            });
        }
    }

    ~CloudStreamWindow() override {
        network.disconnect(this);
        for (auto *reply : network.findChildren<QNetworkReply *>()) {
            reply->disconnect(this);
            reply->abort();
        }
        for (auto *process : findChildren<QProcess *>()) {
            process->disconnect(this);
            if (process->state() != QProcess::NotRunning) {
                process->kill();
                process->waitForFinished(250);
            }
        }
    }

    void selectPage(int index) {
        if (pages && index >= 0 && index < pages->count()) pages->setCurrentIndex(index);
    }

    void openSettingsSectionForPreview(const QString &section) {
        selectPage(4);
        if (settingsPane && !section.trimmed().isEmpty()) settingsPane->showSection(section);
    }

    QWidget *openExtensionsForPreview(int tab = 0) {
        showExtensionManager();
        if (extensionManagerTabs && tab >= 0 && tab < extensionManagerTabs->count()) {
            extensionManagerTabs->setCurrentIndex(tab);
        }
        return extensionManagerDialog;
    }

    QWidget *openProviderPickerForPreview() {
        auto *dialog = new CloudStream::ProviderPickerDialog(
            providerChoices, activeHomeSelectionKey(),
            settings.value("pinnedHomeProviderKeys").toStringList(), centralWidget());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        dialog->raise();
        return this;
    }

    void installAllAvailableForAutomation(std::function<void(int, int)> completion) {
        openExtensionsForPreview(2);
        auto attempts = std::make_shared<int>(0);
        auto poll = std::make_shared<std::function<void()>>();
        *poll = [this, attempts, poll, completion] {
            if (!currentCatalogPlugins.isEmpty()) {
                QTimer::singleShot(0, this, [poll] { *poll = {}; });
                installAllExtensions(true, completion);
                return;
            }
            if (++*attempts >= 300) {
                QTimer::singleShot(0, this, [poll] { *poll = {}; });
                completion(0, 1);
                return;
            }
            QTimer::singleShot(100, this, [poll] { if (*poll) (*poll)(); });
        };
        (*poll)();
    }

    void loadAllHomeSectionsForAutomation(
        std::function<void(int rendered, int total, int manualButtons,
                           qint64 longestBatchMs)> completion) {
        auto attempts = std::make_shared<int>(0);
        auto poll = std::make_shared<std::function<void()>>();
        *poll = [this, attempts, poll, completion] {
            const auto total = progressiveHomeSections.size();
            if (total > 0 && renderedHomeSectionCount >= total) {
                int manualButtons = 0;
                for (auto *candidate : homeSectionsContainer->findChildren<QPushButton *>()) {
                    if (candidate->text().startsWith("Show ") &&
                        candidate->text().contains("more section")) ++manualButtons;
                }
                QTimer::singleShot(0, this, [poll] { *poll = {}; });
                completion(renderedHomeSectionCount, total, manualButtons,
                           longestHomeAppendMs);
                return;
            }
            if (++*attempts >= 600) {
                QTimer::singleShot(0, this, [poll] { *poll = {}; });
                completion(renderedHomeSectionCount, total, -1,
                           longestHomeAppendMs);
                return;
            }
            if (total > 0 && homeScrollArea) {
                auto *bar = homeScrollArea->verticalScrollBar();
                bar->setValue(bar->maximum());
                maybeAppendHomeSections();
            }
            QTimer::singleShot(25, this, [poll] { if (*poll) (*poll)(); });
        };
        (*poll)();
    }

    void searchForAutomation(
        const QString &term,
        std::function<void(int results, qint64 firstResultMs,
                           qint64 totalMs, int peakProcesses)> completion) {
        auto elapsed = std::make_shared<QElapsedTimer>();
        elapsed->start();
        auto firstResultMs = std::make_shared<qint64>(-1);
        auto started = std::make_shared<bool>(false);
        auto attempts = std::make_shared<int>(0);
        auto poll = std::make_shared<std::function<void()>>();
        *poll = [this, term, completion, elapsed, firstResultMs,
                 started, attempts, poll] {
            if (!*started && !providerChoices.isEmpty() && searchAutomation) {
                *started = true;
                selectPage(1);
                searchAutomation(term);
            }
            int validResults = 0;
            if (searchResultsList) {
                for (int row = 0; row < searchResultsList->count(); ++row) {
                    if (!searchResultsList->item(row)->data(Qt::UserRole + 5)
                             .toString().isEmpty()) ++validResults;
                }
            }
            if (validResults > 0 && *firstResultMs < 0) {
                *firstResultMs = elapsed->elapsed();
            }
            if (*started && activeSearchProcesses.isEmpty()) {
                QTimer::singleShot(0, this, [poll] { *poll = {}; });
                completion(validResults, *firstResultMs, elapsed->elapsed(),
                           searchPeakConcurrentProcesses);
                return;
            }
            if (++*attempts >= 1200) {
                QTimer::singleShot(0, this, [poll] { *poll = {}; });
                completion(validResults, *firstResultMs, elapsed->elapsed(),
                           searchPeakConcurrentProcesses);
                return;
            }
            QTimer::singleShot(25, this, [poll] { if (*poll) (*poll)(); });
        };
        (*poll)();
    }

    void openDetailsForPreview(const QString &artifact, const QString &provider, const QString &url) {
        openDetails(artifact, provider, url, false);
    }

    void openSourcesForPreview(const QString &artifact, const QString &provider,
                               const QString &data, const QString &outputPath) {
        sourcePreviewOutputPath = outputPath;
        resolveAndPlay(artifact, provider, data, {}, "Source preview", true);
    }

    void openDownloadSourcesForPreview(const QString &artifact, const QString &provider,
                                       const QString &data, const QString &outputPath) {
        sourcePreviewOutputPath = outputPath;
        resolveAndPlay(artifact, provider, data, {}, "Download preview", true, true);
    }

    void openPlayerForPreview(const QString &mediaPath, const QString &outputPath) {
        CloudStream::SourceDiscovery discovery;
        discovery.success = true;
        CloudStream::PlaybackSource source;
        source.source = "Local preview";
        source.name = "Generated video • 1080p";
        source.url = mediaPath;
        source.type = "VIDEO";
        source.quality = 1080;
        discovery.sources.append(source);
        CloudStream::PlayerPreferences preferences;
        preferences.seekSeconds = settings.value("player/seekSeconds", 10).toInt();
        preferences.initialVolume = settings.value("player/volume", 80).toInt();
        auto *window = new CloudStream::IntegratedPlayerWindow(
            discovery, "CloudStream Player Preview • Episode 1", 0.0, this, preferences);
        integratedPlayer = window;
        window->show();
        QTimer::singleShot(1400, window, [window, outputPath] {
            window->grab().save(outputPath);
            window->close();
        });
    }

    void openProviderConfigurationForPreview(const QString &internalName,
                                             const QString &outputPath) {
        providerConfigurationPreviewOutputPath = outputPath;
        showExtensionManager();
        if (extensionManagerTabs) extensionManagerTabs->setCurrentIndex(1);
        for (int row = 0; installedExtensions && row < installedExtensions->count(); ++row) {
            auto *item = installedExtensions->item(row);
            if (item->data(Qt::UserRole).toString() == internalName) {
                installedExtensions->setCurrentItem(item);
                configureSelectedExtension();
                return;
            }
        }
    }

private:
    QSettings settings;
    CloudStream::ExtensionRegistry extensionRegistry;
    CloudStream::WatchHistoryStore history;
    CloudStream::DownloadQueueStore downloadQueue;
    CloudStream::DownloadManager downloadManager;
    QNetworkAccessManager network;
    CloudStream::ArtworkLoader *artworkLoader{};
    CloudStream::GamepadNavigation *gamepadNavigation{};
    QStackedWidget *pages{};
    QWidget *sidebarPanel{};
    QScrollArea *homeScrollArea{};
    QVBoxLayout *homeSectionsLayout{};
    QComboBox *homeProviderSelector{};
    QPushButton *homeProviderButton{};
    HomeHeroBanner *homeHeroBanner{};
    QWidget *homeOnboarding{};
    QLabel *homeOnboardingTitle{};
    QLabel *homeOnboardingMessage{};
    QWidget *homeSectionsContainer{};
    QLabel *homeHeroTitle{};
    QLabel *homeHeroMeta{};
    QPushButton *homeHeroPlay{};
    QPushButton *homeHeroInfo{};
    QString homeHeroUrl;
    QString homeHeroProvider;
    QString homeHeroJar;
    QString homeHeroArtworkUrl;
    QPushButton *searchProviderButton{};
    QLineEdit *searchInput{};
    QListWidget *searchResultsList{};
    QButtonGroup *searchTypeFilter{};
    QComboBox *searchResultProviderFilter{};
    QLineEdit *homeSearchInput{};
    QWidget *homeSearchPanel{};
    QListWidget *homeSearchResults{};
    bool homeSearchOpen = false;
    QButtonGroup *homeSearchTypeFilter{};
    QPointer<QProcess> activeHomeSearchProcess;
    QPointer<QObject> homeSearchArtworkContext;
    quint64 homeSearchGeneration = 0;
    std::function<void(const QString &)> searchAutomation;
    int searchPeakConcurrentProcesses = 0;
    QList<QPushButton *> navigationButtons;
    QListWidget *repositories{};
    QListWidget *extensions{};
    QPushButton *installAllExtensionsButton{};
    QListWidget *installedExtensions{};
    QDialog *extensionManagerDialog{};
    QLineEdit *extensionSearch{};
    QTabBar *extensionManagerTabs{};
    QLabel *selectedRepositoryLabel{};
    QPushButton *openRepositoryButton{};
    QPushButton *removeRepositoryButton{};
    QLabel *extensionCatalogContext{};
    QLabel *installedExtensionDetailIcon{};
    QLabel *catalogExtensionDetailIcon{};
    QString sourcePreviewOutputPath;
    QString providerConfigurationPreviewOutputPath;
    QList<CloudStream::PluginInfo> currentCatalogPlugins;
    QString currentCatalogRepositoryUrl;
    bool bulkExtensionInstallActive = false;
    QLabel *status{};
    QLineEdit *urlInput{};
    QProcess *playerProcess{};
    QPointer<CloudStream::IntegratedPlayerWindow> integratedPlayer;
    QPointer<QDialog> activeDetailsDialog;
    QLocalSocket *playerIpc{};
    QTimer *playerProgressTimer{};
    QWidget *continueWatchingCard{};
    QListWidget *continueWatchingList{};
    std::function<void()> libraryRefresh;
    bool libraryLoaded = false;
    std::function<void()> downloadsRefresh;
    CloudStream::SettingsPane *settingsPane{};
    QByteArray playerIpcBuffer;
    QString activeHistoryId;
    double activePositionSeconds = 0.0;
    double activeDurationSeconds = 0.0;
    bool homeContentRendered = false;
    bool resetHomeScrollPending = false;
    bool homeAppendScheduled = false;
    bool homeAppendingSections = false;
    int renderedHomeSectionCount = 0;
    quint64 homeSectionGeneration = 0;
    qint64 longestHomeAppendMs = 0;
    QJsonArray progressiveHomeSections;
    QString progressiveHomeProviderName;
    QString currentHomeJar;
    QString currentHomeProviderKey;
    QList<QJsonObject> providerChoices;
    QList<QJsonObject> automaticHomeProviderChoices;
    QList<QJsonObject> allProviderChoices;
    QList<QJsonObject> stagedProviderChoices;
    CloudStream::ProviderDiscoveryGeneration providerDiscoveryGeneration;
    CloudStream::ProviderDiscoveryGeneration searchRequestGeneration;
    CloudStream::ProviderDiscoveryGeneration homeRequestGeneration;
    CloudStream::ProviderDiscoveryGeneration playbackRequestGeneration;
    QList<QPointer<QProcess>> activeSearchProcesses;
    QPointer<QProcess> activeHomeProcess;
    QPointer<QProcess> activePlaybackResolutionProcess;
    QPointer<QObject> searchArtworkContext;
    QSet<QString> convertingExtensionSources;
    QString player() const { return settings.value("player", "mpv").toString(); }

    void applyInterfaceLayout() {
        if (!sidebarPanel) return;
        const auto layout = settings.value("interface/layout", "Compact side rail").toString();
        const bool expanded = layout == "Expanded side navigation";
        sidebarPanel->setVisible(layout != "Focus content");
        sidebarPanel->setFixedWidth(expanded ? 220 : 76);
        const QStringList labels{"Home", "Search", "Library", "Downloads", "Settings"};
        for (int index = 0; index < navigationButtons.size(); ++index) {
            auto *button = navigationButtons[index];
            button->setFixedSize(expanded ? 200 : 52, 46);
            button->setText(expanded ? labels.value(index) : QString());
            button->setProperty("expanded", expanded);
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }

    void applyAppearance() {
        if (auto *application = qobject_cast<QApplication *>(QApplication::instance())) {
            const auto theme = settings.value("interface/theme", "Android dark").toString();
            const auto density = settings.value("interface/density", "Comfortable").toString();
            application->setStyleSheet(theme == "Android dark" && density == "Comfortable"
                ? defaultApplicationStyleSheet
                : defaultApplicationStyleSheet + appearanceOverrides(settings));
        }
        applyInterfaceLayout();
    }

    void startStartupAnimation() {
        if (!centralWidget()) return;
        auto *overlay = new QWidget(centralWidget());
        overlay->setObjectName("startupOverlay");
        overlay->setGeometry(centralWidget()->rect());
        overlay->setStyleSheet(
            "QWidget#startupOverlay{background:#07080d;}"
            "QLabel#startupLogo{background:transparent;}"
            "QLabel#startupTitle{background:transparent;color:#f4f3fa;font-size:24px;font-weight:750;}"
            "QLabel#startupMessage{background:transparent;color:#9da0ac;font-size:13px;}"
        );
        auto *layout = new QVBoxLayout(overlay);
        layout->setContentsMargins(30, 30, 30, 30);
        layout->setSpacing(12);
        layout->addStretch();
        auto *logo = new QLabel(overlay);
        logo->setObjectName("startupLogo");
        logo->setPixmap(QPixmap(":/assets/cloudstream-launcher.png").scaled(
            112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logo->setAlignment(Qt::AlignCenter);
        layout->addWidget(logo, 0, Qt::AlignHCenter);
        auto *heading = new QLabel("CloudStream", overlay);
        heading->setObjectName("startupTitle");
        heading->setAlignment(Qt::AlignCenter);
        layout->addWidget(heading);
        auto *message = new QLabel("Loading your streaming providers…", overlay);
        message->setObjectName("startupMessage");
        message->setAlignment(Qt::AlignCenter);
        layout->addWidget(message);
        layout->addStretch();
        overlay->raise();
        auto *effect = new QGraphicsOpacityEffect(overlay);
        overlay->setGraphicsEffect(effect);
        auto *fade = new QPropertyAnimation(effect, "opacity", overlay);
        fade->setDuration(520);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        connect(fade, &QPropertyAnimation::finished, overlay, &QWidget::deleteLater);
        QTimer::singleShot(260, fade, [fade] { fade->start(); });
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QMainWindow::resizeEvent(event);
        if (pages && activeDetailsDialog && activeDetailsDialog->parentWidget() == pages)
            activeDetailsDialog->setGeometry(pages->rect());
        if (pages && integratedPlayer && integratedPlayer->parentWidget() == pages)
            integratedPlayer->setGeometry(pages->rect());
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape && pages && pages->currentIndex() != 0 &&
            !QApplication::activeModalWidget()) {
            pages->setCurrentIndex(0);
            if (!navigationButtons.isEmpty()) {
                navigationButtons.first()->setChecked(true);
                navigationButtons.first()->setFocus(Qt::BacktabFocusReason);
            }
            event->accept();
            return;
        }
        QMainWindow::keyPressEvent(event);
    }

private:
    bool playerHasControllerFocus() const {
        if (!integratedPlayer || !integratedPlayer->isVisible()) return false;
        return QApplication::activeWindow() == integratedPlayer;
    }

    bool embeddedOverlayIsVisible() const {
        if (!centralWidget()) return false;
        for (auto *overlay : centralWidget()->findChildren<CloudStream::ProviderPickerDialog *>()) {
            if (overlay->isVisible()) return true;
        }
        return false;
    }

    void sendControllerKey(int key) {
        auto *target = QApplication::focusWidget();
        if (!target) target = QApplication::activeWindow();
        if (!target) return;
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
        QApplication::sendEvent(target, &press);
        QApplication::sendEvent(target, &release);
    }

    void switchControllerPage(int direction) {
        if (!pages || navigationButtons.isEmpty()) return;
        const auto count = pages->count();
        const auto target = (pages->currentIndex() + direction + count) % count;
        pages->setCurrentIndex(target);
        navigationButtons.value(target)->setChecked(true);
        navigationButtons.value(target)->setFocus(Qt::TabFocusReason);
    }

    void setupGamepadNavigation() {
        gamepadNavigation = new CloudStream::GamepadNavigation(this);
        connect(gamepadNavigation, &CloudStream::GamepadNavigation::actionTriggered,
                this, [this](CloudStream::GamepadNavigation::Action action) {
            const auto playerFocused = playerHasControllerFocus();
            if (!playerFocused &&
                (QApplication::activeWindow() != this || embeddedOverlayIsVisible())) return;
            switch (action) {
            case CloudStream::GamepadNavigation::SearchOrFullscreen:
                if (playerFocused) sendControllerKey(Qt::Key_F);
                else if (pages) {
                    pages->setCurrentIndex(1);
                    if (searchInput) searchInput->setFocus(Qt::ShortcutFocusReason);
                }
                break;
            case CloudStream::GamepadNavigation::Secondary:
                if (playerFocused) sendControllerKey(Qt::Key_M);
                else sendControllerKey(Qt::Key_Menu);
                break;
            case CloudStream::GamepadNavigation::PreviousPage:
                if (playerFocused) sendControllerKey(Qt::Key_Left);
                else switchControllerPage(-1);
                break;
            case CloudStream::GamepadNavigation::NextPage:
                if (playerFocused) sendControllerKey(Qt::Key_Right);
                else switchControllerPage(1);
                break;
            default:
                break;
            }
        });
        connect(gamepadNavigation, &CloudStream::GamepadNavigation::controllerConnected,
                this, [this](const QString &name) {
            if (status) status->setText((name.isEmpty() ? "Controller" : name) +
                                        " connected • D-pad / A / B ready");
        });
        connect(gamepadNavigation, &CloudStream::GamepadNavigation::controllerDisconnected,
                this, [this] {
            if (status) status->setText("Controller disconnected");
        });
        if (gamepadNavigation->controllerCount() > 0 && status) {
            status->setText("Controller connected • D-pad / A / B ready");
        }
    }

    void migrateExtensionState() {
        extensionRegistry.synchronizeArtifacts(CloudStream::XdgPaths::extensionsDir());
        const auto legacyRepository = settings.value("repository").toString().trimmed();
        if (!legacyRepository.isEmpty()) {
            extensionRegistry.addRepository({"Imported repository", legacyRepository});
            settings.setValue("selectedRepositoryUrl", legacyRepository);
            settings.remove("repository");
        }
    }

    QString activeHomeSelectionKey() const {
        if (!homeProviderSelector) return "none";
        const auto descriptor = QJsonObject::fromVariantMap(
            homeProviderSelector->currentData().toMap());
        return descriptor.value("mode").toString() == "provider"
            ? providerKey(descriptor)
            : descriptor.value("mode").toString("none");
    }

    void updateHomeProviderButton() {
        if (!homeProviderButton || !homeProviderSelector) return;
        const auto descriptor = QJsonObject::fromVariantMap(
            homeProviderSelector->currentData().toMap());
        const auto mode = descriptor.value("mode").toString();
        QString fullLabel;
        if (mode == "provider") fullLabel = providerDisplayName(descriptor);
        else if (mode == "random") fullLabel = "Random provider";
        else fullLabel = "No provider";
        const auto label = homeProviderButton->fontMetrics().elidedText(
            fullLabel, Qt::ElideRight, 228);
        homeProviderButton->setText(label + "   ▾");
        homeProviderButton->setToolTip("Home provider: " + fullLabel);
        homeProviderButton->setAccessibleName("Home provider: " + fullLabel);
    }

    void showHomeProviderDialog() {
        if (!homeProviderButton || !homeProviderButton->isEnabled()) return;
        CloudStream::ProviderPickerDialog dialog(
            providerChoices, activeHomeSelectionKey(),
            settings.value("pinnedHomeProviderKeys").toStringList(), centralWidget());
        const auto result = dialog.exec();
        settings.setValue("pinnedHomeProviderKeys", dialog.pinnedProviderKeys());
        if (result != CloudStream::ProviderPickerDialog::Accepted) {
            homeProviderButton->setFocus();
            return;
        }
        const auto selectedKey = dialog.selectedKey();
        for (int index = 0; index < homeProviderSelector->count(); ++index) {
            const auto descriptor = QJsonObject::fromVariantMap(
                homeProviderSelector->itemData(index).toMap());
            const auto key = descriptor.value("mode").toString() == "provider"
                ? providerKey(descriptor)
                : descriptor.value("mode").toString();
            if (key != selectedKey) continue;
            homeProviderSelector->setCurrentIndex(index);
            updateHomeProviderButton();
            break;
        }
        homeProviderButton->setFocus();
    }

    QJsonObject selectedProvider() const {
        if (!homeProviderSelector) return {};
        auto descriptor = QJsonObject::fromVariantMap(homeProviderSelector->currentData().toMap());
        if (descriptor.value("mode").toString() == "random") {
            if (automaticHomeProviderChoices.isEmpty()) return {};
            return automaticHomeProviderChoices[
                QRandomGenerator::global()->bounded(automaticHomeProviderChoices.size())];
        }
        return descriptor.value("mode").toString() == "provider" ? descriptor : QJsonObject();
    }

    QString providerKey(const QJsonObject &provider) const {
        return CloudStream::ProviderSelectionModel::key(provider);
    }

    QString providerDisplayName(const QJsonObject &provider) const {
        return CloudStream::ProviderSelectionModel::displayName(provider);
    }

    bool isNsfwProvider(const QString &jar, const QString &name) const {
        for (const auto &provider : allProviderChoices) {
            if (provider.value("jarPath").toString() != jar ||
                provider.value("name").toString() != name) continue;
            for (const auto &type : provider.value("supportedTypes").toArray()) {
                if (type.toString().compare("NSFW", Qt::CaseInsensitive) == 0) return true;
            }
        }
        return false;
    }

    QList<QJsonObject> selectedSearchProviders() const {
        return CloudStream::ProviderSelectionModel::effectiveSearchCandidates(
            allProviderChoices, settings.value("searchProviderKeys").toStringList(),
            settings.value("providers/language", "All languages").toString(),
            settings.value("providers/allowNsfw", false).toBool());
    }

    void refreshSearchProviderButton() {
        const auto selected = selectedSearchProviders();
        if (searchResultProviderFilter) {
            const auto previous = searchResultProviderFilter->currentData().toString();
            const QSignalBlocker blocker(searchResultProviderFilter);
            searchResultProviderFilter->clear();
            searchResultProviderFilter->addItem("All providers", "");
            QSet<QString> names;
            for (const auto &provider : selected) {
                const auto name = provider.value("name").toString();
                if (!name.isEmpty() && !names.contains(name)) {
                    names.insert(name);
                    searchResultProviderFilter->addItem(name, name);
                }
            }
            const auto index = searchResultProviderFilter->findData(previous);
            searchResultProviderFilter->setCurrentIndex(index >= 0 ? index : 0);
        }
        if (!searchProviderButton) return;
        const bool all = settings.value("searchProviderKeys").toStringList().isEmpty();
        searchProviderButton->setText(all
            ? "Providers: Automatic (" + QString::number(selected.size()) + ")"
            : "Providers: " + QString::number(selected.size()) + " selected");
        searchProviderButton->setEnabled(!allProviderChoices.isEmpty());
    }

    bool showSearchProviderDialog() {
        QDialog dialog(this);
        dialog.setObjectName("searchProviderDialog");
        dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dialog.setWindowTitle("Search providers");
        dialog.resize(460, 560);
        dialog.setStyleSheet(
            "QDialog#searchProviderDialog{background:#0b0b0e;color:#f5f4f8;border:1px solid #292d39;border-radius:22px;}"
            "QLabel{background:transparent;}"
            "QListWidget{background:#101218;color:#eeeeF2;border:1px solid #292d39;border-radius:16px;padding:7px;}"
            "QListWidget::item{padding:9px 12px;border-radius:10px;}"
            "QListWidget::item:hover{background:#1c202a;}"
            "QListWidget::item:selected{background:#1b2754;color:white;}"
            "QListWidget::indicator{width:18px;height:18px;border-radius:6px;border:1px solid #545a6b;background:#181b24;}"
            "QListWidget::indicator:checked{background:#536dfe;border:1px solid #6f83ff;}"
            "QPushButton{min-height:40px;padding:0 16px;background:#222632;color:#f5f4f8;border:0;border-radius:20px;}"
            "QPushButton:hover{background:#30384a;}"
            "QDialogButtonBox QPushButton{min-width:92px;}"
            "QPushButton[primary=\"true\"]{background:#536dfe;color:white;}"
        );
        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(22, 20, 22, 20);
        layout->setSpacing(12);
        layout->addWidget(title("Choose search providers", 20));
        auto *description = new QLabel("Search can query several installed providers at once. This selection is independent from Home.");
        description->setWordWrap(true);
        description->setStyleSheet("color:#aaaab8;");
        layout->addWidget(description);
        auto *list = new QListWidget;
        const auto savedKeys = settings.value("searchProviderKeys").toStringList();
        const QSet<QString> selectedKeys(savedKeys.begin(), savedKeys.end());
        const auto selectableProviders = CloudStream::ProviderPreferenceFilter::apply(
            allProviderChoices, settings.value("providers/language", "All languages").toString(),
            settings.value("providers/allowNsfw", false).toBool());
        QSet<QString> selectableKeys;
        for (const auto &provider : selectableProviders) selectableKeys.insert(providerKey(provider));
        for (const auto &provider : selectableProviders) {
            const auto key = providerKey(provider);
            auto *item = new QListWidgetItem(providerDisplayName(provider));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState((savedKeys.isEmpty() || selectedKeys.contains(key))
                ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, key);
            list->addItem(item);
        }
        layout->addWidget(list, 1);
        auto *selectAll = button("Select all");
        selectAll->setMaximumWidth(150);
        selectAll->setProperty("primary", true);
        connect(selectAll, &QPushButton::clicked, list, [list] {
            for (int row = 0; row < list->count(); ++row) list->item(row)->setCheckState(Qt::Checked);
        });
        layout->addWidget(selectAll);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        if (dialog.exec() != QDialog::Accepted) return false;
        QStringList selected;
        for (int row = 0; row < list->count(); ++row) {
            if (list->item(row)->checkState() == Qt::Checked) selected << list->item(row)->data(Qt::UserRole).toString();
        }
        if (selected.isEmpty()) {
            QMessageBox::information(this, "Choose a provider", "Select at least one provider to search.");
            return false;
        }
        if (QSet<QString>(selected.begin(), selected.end()) == selectableKeys) selected.clear();
        settings.setValue("searchProviderKeys", selected);
        refreshSearchProviderButton();
        return true;
    }

    void stageValidatedProviders(const CloudStream::ExtensionRecord &extension,
                                 const QJsonArray &providers) {
        stagedProviderChoices = CloudStream::ProviderSelectionModel::mergeValidatedProviders(
            stagedProviderChoices, extension.internalName, extension.artifactPath, providers);
    }

    void publishStagedProviders() {
        if (stagedProviderChoices.isEmpty()) return;
        providerDiscoveryGeneration.begin();
        const auto publishedCount = stagedProviderChoices.size();
        QSet<QString> updatedExtensions;
        for (const auto &provider : stagedProviderChoices) {
            updatedExtensions.insert(provider.value("extensionName").toString());
        }
        QList<QJsonObject> merged;
        for (const auto &provider : allProviderChoices) {
            if (!updatedExtensions.contains(provider.value("extensionName").toString())) merged << provider;
        }
        merged << stagedProviderChoices;
        stagedProviderChoices.clear();
        finishProviderChoices(merged, true);
        qInfo() << "Published" << publishedCount << "newly validated provider choice(s)";
    }

    void finishProviderChoices(QList<QJsonObject> choices,
                               bool preserveHomeIfSelectionUnchanged = false) {
        std::sort(choices.begin(), choices.end(), [](const auto &left, const auto &right) {
            return left.value("name").toString().localeAwareCompare(right.value("name").toString()) < 0;
        });
        allProviderChoices = choices;
        const auto language = settings.value("providers/language", "All languages").toString();
        const auto allowNsfw = settings.value("providers/allowNsfw", false).toBool();
        providerChoices = CloudStream::ProviderSelectionModel::selectableHomeCandidates(
            allProviderChoices, language, allowNsfw);
        automaticHomeProviderChoices = CloudStream::ProviderSelectionModel::automaticHomeCandidates(
            allProviderChoices, language, allowNsfw);
        const auto savedSearchKeys = settings.value("searchProviderKeys").toStringList();
        if (!savedSearchKeys.isEmpty()) {
            QSet<QString> allowedSearchKeys;
            for (const auto &provider : CloudStream::ProviderPreferenceFilter::apply(
                     allProviderChoices, language, allowNsfw)) {
                allowedSearchKeys.insert(providerKey(provider));
            }
            QStringList retainedSearchKeys;
            for (const auto &key : savedSearchKeys) {
                if (allowedSearchKeys.contains(key)) retainedSearchKeys << key;
            }
            if (retainedSearchKeys.isEmpty()) settings.remove("searchProviderKeys");
            else if (retainedSearchKeys != savedSearchKeys) {
                settings.setValue("searchProviderKeys", retainedSearchKeys);
            }
        }
        qInfo() << "Provider choices:" << providerChoices.size() << "selectable Home of" <<
                   allProviderChoices.size() << "discovered," <<
                   automaticHomeProviderChoices.size() << "automatic Home candidate(s)";
        refreshSearchProviderButton();
        if (!homeProviderSelector) return;
        const auto savedKey = settings.value("homeProviderKey").toString();
        homeProviderSelector->blockSignals(true);
        homeProviderSelector->clear();
        homeProviderSelector->addItem("None", QVariantMap{{"mode", "none"}});
        homeProviderSelector->addItem("Random", QVariantMap{{"mode", "random"}});
        QSet<QString> automaticKeys;
        for (const auto &provider : automaticHomeProviderChoices) automaticKeys.insert(providerKey(provider));
        int selectedIndex = savedKey == "none" ? 0 : savedKey == "random" ? 1 : -1;
        for (const auto &provider : providerChoices) {
            homeProviderSelector->addItem(providerDisplayName(provider), provider.toVariantMap());
            const auto index = homeProviderSelector->count() - 1;
            if (!automaticKeys.contains(providerKey(provider))) {
                homeProviderSelector->setItemData(index,
                    "Available for explicit selection; excluded from automatic choices by provider preferences",
                    Qt::ToolTipRole);
            }
            if (providerKey(provider) == savedKey) selectedIndex = homeProviderSelector->count() - 1;
        }
        if (selectedIndex < 0 && !automaticHomeProviderChoices.isEmpty()) {
            const auto automaticKey = providerKey(automaticHomeProviderChoices.first());
            for (int index = 2; index < homeProviderSelector->count(); ++index) {
                const auto provider = QJsonObject::fromVariantMap(homeProviderSelector->itemData(index).toMap());
                if (providerKey(provider) == automaticKey) {
                    selectedIndex = index;
                    break;
                }
            }
        }
        homeProviderSelector->setCurrentIndex(std::max(0, selectedIndex));
        const auto resolvedProvider = QJsonObject::fromVariantMap(
            homeProviderSelector->currentData().toMap());
        const auto resolvedKey = resolvedProvider.value("mode").toString() == "provider"
            ? providerKey(resolvedProvider)
            : resolvedProvider.value("mode").toString("none");
        if (resolvedKey != savedKey) settings.setValue("homeProviderKey", resolvedKey);
        homeProviderSelector->setEnabled(true);
        homeProviderSelector->blockSignals(false);
        if (homeProviderButton) {
            homeProviderButton->setEnabled(true);
            updateHomeProviderButton();
        }
        const auto selectedProvider = QJsonObject::fromVariantMap(
            homeProviderSelector->currentData().toMap());
        if (!preserveHomeIfSelectionUnchanged ||
            selectedProvider.value("mode").toString() != "provider" ||
            CloudStream::ProviderSelectionModel::shouldReloadHome(
                currentHomeProviderKey, selectedProvider)) {
            loadHomeContent();
        }
    }

    void convertAndroidExtension(const CloudStream::ExtensionRecord &record,
                                 bool selectOnHome = false,
                                 std::function<void(bool)> completion = {}) {
        const auto source = record.sourceArtifactPath.isEmpty() ? record.artifactPath : record.sourceArtifactPath;
        if (!QFileInfo::exists(source) || QFileInfo(source).suffix().compare("cs3", Qt::CaseInsensitive) != 0 ||
            convertingExtensionSources.contains(source)) {
            if (completion) completion(false);
            return;
        }
        const auto helper = CloudStream::ProviderHostCommand::discover();
        if (helper.isEmpty()) {
            if (status) status->setText("Provider host is required to convert Android extensions");
            if (completion) completion(false);
            return;
        }
        convertingExtensionSources.insert(source);
        auto safeName = record.internalName;
        safeName.replace(QRegularExpression("[^A-Za-z0-9_.-]"), "_");
        const auto repositoryId = QString::fromLatin1(QCryptographicHash::hash(
            record.repositoryUrl.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
        const auto outputDirectory = CloudStream::XdgPaths::extensionsDir() + "/converted/" +
            repositoryId + "/" + safeName + "/" + QString::number(std::max(0, record.version));
        QDir().mkpath(outputDirectory);
        const auto output = outputDirectory + "/" + safeName + ".jar";
        if (status) status->setText("Converting " + record.displayName + " for Linux…");
        auto *conversion = new QProcess(this);
        helper.configure(conversion, {"convert", source, output});
        CloudStream::ProcessCompletion::watch(conversion, this,
                [this, conversion, helper, record, source, output, selectOnHome, completion]
                (int exitCode, QProcess::ExitStatus, bool) {
            if (exitCode != 0 || !QFileInfo::exists(output)) {
                convertingExtensionSources.remove(source);
                qWarning().noquote() << "Extension conversion failed:" << conversion->readAllStandardError();
                if (status) status->setText("Could not convert " + record.displayName + " for Linux");
                conversion->deleteLater();
                refreshInstalledExtensionsUi();
                if (completion) completion(false);
                return;
            }
            conversion->deleteLater();
            auto *validation = new QProcess(this);
            helper.configure(validation, {"list", output, "auto"});
            CloudStream::ProcessCompletion::watch(validation, this,
                    [this, validation, record, source, output, selectOnHome, completion]
                    (int validationExit, QProcess::ExitStatus, bool) {
                const auto providers = QJsonDocument::fromJson(validation->readAllStandardOutput()).array();
                const auto validationError = QString::fromUtf8(validation->readAllStandardError()).trimmed();
                const auto validationResult = CloudStream::ProviderValidation::classify(
                    validationExit, providers.size(), validationError);
                convertingExtensionSources.remove(source);
                if (validationResult != CloudStream::ProviderValidationResult::Runnable) {
                    qWarning().noquote() << "Converted extension validation failed:" << validationError;
                    auto retained = record;
                    for (const auto &latest : extensionRegistry.extensions()) {
                        if (latest.internalName == record.internalName &&
                            (latest.sourceArtifactPath == source || latest.artifactPath == source)) {
                            retained = latest;
                            break;
                        }
                    }
                    retained.sourceArtifactPath = source;
                    retained.converterId = converterId;
                    retained.enabled = false;
                    if (validationResult == CloudStream::ProviderValidationResult::RequiresConfiguration) {
                        retained.artifactPath = output;
                        retained.platform = "jvm-configurable";
                        if (status) status->setText("Installed " + record.displayName + " • provider settings required");
                    } else {
                        QFile::remove(output);
                        retained.artifactPath = source;
                        if (validationResult == CloudStream::ProviderValidationResult::UtilityOnly) {
                            retained.platform = "android-utility";
                            if (status) status->setText("Stored " + record.displayName + " • utility/integration extension (no media provider)");
                        } else if (validationResult == CloudStream::ProviderValidationResult::AndroidOnly) {
                            retained.platform = "android-incompatible";
                            if (status) status->setText("Stored " + record.displayName + " • requires an Android runtime or UI");
                        } else {
                            retained.platform = "android-failed";
                            if (status) status->setText("Stored " + record.displayName + " • Linux conversion failed validation");
                        }
                    }
                    extensionRegistry.upsertExtension(retained);
                    validation->deleteLater();
                    refreshInstalledExtensionsUi();
                    if (completion) completion(true);
                    return;
                }
                auto converted = record;
                for (const auto &latest : extensionRegistry.extensions()) {
                    if (latest.internalName == record.internalName &&
                        (latest.sourceArtifactPath == source || latest.artifactPath == source)) {
                        converted = latest;
                        break;
                    }
                }
                converted.sourceArtifactPath = source;
                converted.artifactPath = output;
                converted.platform = "jvm-converted";
                converted.converterId = converterId;
                converted.enabled = true;
                extensionRegistry.upsertExtension(converted);
                stageValidatedProviders(converted, providers);
                if (selectOnHome) {
                    settings.setValue("homeProviderKey", output + "\n" + providers.first().toObject().value("name").toString());
                }
                if (status) status->setText("Enabled " + converted.displayName + " on Linux • " +
                    QString::number(providers.size()) + " provider(s)");
                validation->deleteLater();
                refreshInstalledExtensionsUi();
                if (!completion) publishStagedProviders();
                if (!completion) loadExtensions();
                if (completion) completion(true);
            });
            validation->start();
            QTimer::singleShot(15000, validation, [validation] {
                if (validation->state() != QProcess::NotRunning) validation->kill();
            });
        });
        conversion->start();
        QTimer::singleShot(30000, conversion, [conversion] {
            if (conversion->state() != QProcess::NotRunning) conversion->kill();
        });
    }

    bool convertDownloadedAndroidExtensions() {
        QList<CloudStream::ExtensionRecord> pendingConversions;
        for (const auto &extension : extensionRegistry.extensions()) {
            const auto source = extension.sourceArtifactPath.isEmpty() ? extension.artifactPath : extension.sourceArtifactPath;
            if (extension.platform == "android" ||
                (extension.converterId != converterId && QFileInfo(source).suffix().compare("cs3", Qt::CaseInsensitive) == 0)) {
                pendingConversions << extension;
            }
        }
        if (pendingConversions.isEmpty()) return false;

        auto queue = std::make_shared<QList<CloudStream::ExtensionRecord>>(pendingConversions);
        auto index = std::make_shared<int>(0);
        auto next = std::make_shared<std::function<void()>>();
        *next = [this, queue, index, next] {
            if (*index >= queue->size()) {
                refreshInstalledExtensionsUi();
                publishStagedProviders();
                refreshProviderChoices(true);
                QTimer::singleShot(0, this, [next] { *next = {}; });
                return;
            }
            const auto extension = queue->at((*index)++);
            if (status) {
                status->setText("Converting extension " + QString::number(*index) + " of " +
                                QString::number(queue->size()) + " • " + extension.displayName);
            }
            convertAndroidExtension(extension, false, [this, next](bool) {
                QTimer::singleShot(0, this, [next] { if (*next) (*next)(); });
            });
        };
        (*next)();
        return true;
    }

    void refreshProviderChoices(bool preserveHomeIfSelectionUnchanged = false) {
        const auto generation = providerDiscoveryGeneration.begin();
        extensionRegistry.synchronizeArtifacts(CloudStream::XdgPaths::extensionsDir());
        const auto helper = CloudStream::ProviderHostCommand::discover();
        QList<CloudStream::ExtensionRecord> runnable;
        for (const auto &extension : extensionRegistry.extensions()) {
            if (extension.platform.startsWith("jvm") && extension.enabled && QFileInfo::exists(extension.artifactPath)) runnable << extension;
        }
        if (helper.isEmpty() || runnable.isEmpty()) {
            finishProviderChoices({}, preserveHomeIfSelectionUnchanged);
            return;
        }
        auto pending = std::make_shared<int>(runnable.size());
        auto choices = std::make_shared<QList<QJsonObject>>();
        for (const auto &extension : runnable) {
            auto *process = new QProcess(this);
            helper.configure(process, {"list", extension.artifactPath, "auto"});
            CloudStream::ProcessCompletion::watch(process, this,
                    [this, process, extension, pending, choices, generation,
                     preserveHomeIfSelectionUnchanged](int exitCode, QProcess::ExitStatus, bool) {
                if (exitCode == 0) {
                    const auto providers = QJsonDocument::fromJson(process->readAllStandardOutput()).array();
                    for (const auto &value : providers) {
                        const auto object = value.toObject();
                        QJsonObject descriptor = object;
                        descriptor.insert("mode", "provider");
                        descriptor.insert("jarPath", extension.artifactPath);
                        descriptor.insert("extensionName", extension.internalName);
                        choices->append(descriptor);
                    }
                }
                process->deleteLater();
                --*pending;
                if (*pending == 0 && providerDiscoveryGeneration.isCurrent(generation)) {
                    finishProviderChoices(*choices, preserveHomeIfSelectionUnchanged);
                }
            });
            process->start();
            QTimer::singleShot(15000, process, [process] {
                if (process->state() != QProcess::NotRunning) process->kill();
            });
        }
    }

    void build() {
        auto *root = new QWidget;
        root->setObjectName("appRoot");
        auto *layout = new QHBoxLayout(root);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(sidebar());
        status = new QLabel;
        status->setContentsMargins(12, 0, 12, 0);
        statusBar()->addWidget(status);
        statusBar()->setSizeGripEnabled(false);
        statusBar()->setFixedHeight(30);
        statusBar()->hide();
        auto previousStatus = std::make_shared<QString>();
        auto *statusMonitor = new QTimer(this);
        statusMonitor->setInterval(100);
        connect(statusMonitor, &QTimer::timeout, this, [this, previousStatus] {
            const auto message = status ? status->text() : QString();
            if (message == *previousStatus) return;
            *previousStatus = message;
            if (message.isEmpty()) {
                statusBar()->hide();
                return;
            }
            statusBar()->show();
            QTimer::singleShot(4500, this, [this, message] {
                if (status && status->text() == message) statusBar()->hide();
            });
        });
        statusMonitor->start();
        pages = new QStackedWidget;
        pages->setObjectName("appPages");
        pages->addWidget(homePage());
        pages->addWidget(searchPage());
        pages->addWidget(libraryPage());
        pages->addWidget(downloadsPage());
        pages->addWidget(settingsPage());
        pages->setCurrentIndex(0);
        connect(pages, &QStackedWidget::currentChanged, this, [this](int index) {
            if (index >= 0 && index < navigationButtons.size()) navigationButtons[index]->setChecked(true);
            if (index == 2 && !libraryLoaded) {
                libraryLoaded = true;
                QTimer::singleShot(0, this, [this] {
                    if (libraryRefresh) libraryRefresh();
                });
            }
        });
        for (int i = 0; i < 5; ++i) {
            auto *shortcut = new QShortcut(QKeySequence("Ctrl+" + QString::number(i + 1)), this);
            connect(shortcut, &QShortcut::activated, this, [this, i] { pages->setCurrentIndex(i); });
        }
        layout->addWidget(pages, 1);
        setCentralWidget(root);
        refreshContinueWatching();
        QTimer::singleShot(0, this, [this] {
            if (homeScrollArea) homeScrollArea->verticalScrollBar()->setValue(0);
        });
    }

    QWidget *sidebar() {
        auto *panel = new QWidget;
        sidebarPanel = panel;
        panel->setObjectName("sidebar");
        panel->setFixedWidth(76);
        auto *v = new QVBoxLayout(panel);
        v->setContentsMargins(10, 18, 10, 14);
        v->setSpacing(10);
        auto *logo = new QLabel;
        logo->setPixmap(QPixmap(":/assets/cloudstream-launcher.png").scaled(46, 46, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logo->setFixedSize(56, 54);
        logo->setAlignment(Qt::AlignCenter);
        v->addWidget(logo, 0, Qt::AlignHCenter);
        v->addSpacing(14);
        const QStringList labels = {"Home", "Search", "Library", "Downloads", "Settings"};
        const QList<QPair<QString, QString>> icons = {
            {":/icons/home-outline.svg", ":/icons/home-filled.svg"},
            {":/icons/search.svg", ":/icons/search-active.svg"},
            {":/icons/library-outline.svg", ":/icons/library-filled.svg"},
            {":/icons/download.svg", ":/icons/download-active.svg"},
            {":/icons/settings-outline.svg", ":/icons/settings-filled.svg"},
        };
        for (int i = 0; i < labels.size(); ++i) {
            auto *nav = new QPushButton;
            nav->setCheckable(true);
            nav->setAutoExclusive(true);
            nav->setProperty("nav", true);
            nav->setProperty("page", i);
            nav->setFixedSize(52, 46);
            nav->setToolTip(labels[i] + "  ·  Ctrl+" + QString::number(i + 1));
            nav->setAccessibleName(labels[i]);
            QIcon icon;
            icon.addFile(icons[i].first, {}, QIcon::Normal, QIcon::Off);
            icon.addFile(icons[i].second, {}, QIcon::Normal, QIcon::On);
            nav->setIcon(icon);
            nav->setIconSize(QSize(24, 24));
            if (i == 0) nav->setChecked(true);
            navigationButtons << nav;
            connect(nav, &QPushButton::toggled, this, [this, i](bool checked) {
                if (checked && pages) pages->setCurrentIndex(i);
            });
            v->addWidget(nav, 0, Qt::AlignHCenter);
        }
        v->addStretch();
        return panel;
    }

    QWidget *pageFrame(const QString &heading, const QString &description) {
        Q_UNUSED(description);
        auto *page = new QWidget;
        auto *v = new QVBoxLayout(page);
        v->setContentsMargins(30, 26, 30, 28);
        v->setSpacing(16);
        auto *headingLabel = title(heading, 23);
        headingLabel->setObjectName("pageTitle");
        v->addWidget(headingLabel);
        return page;
    }

    QWidget *scrollPage(QWidget *content) {
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(content);
        CloudStream::SmoothScrollController::attach(scroll);
        return scroll;
    }

    QWidget *homePage() {
        auto *page = new QWidget;
        auto *v = new QVBoxLayout(page);
        v->setContentsMargins(28, 20, 28, 30);
        v->setSpacing(20);

        auto *toolbar = new QHBoxLayout;
        toolbar->setSpacing(10);
        toolbar->addWidget(title("Home", 24));
        toolbar->addStretch();
        auto *search = new QToolButton;
        search->setObjectName("topIconButton");
        search->setAccessibleName("Search");
        search->setToolTip("Search providers  ·  Ctrl+2");
        search->setIcon(QIcon(":/icons/search.svg"));
        search->setFixedSize(42, 42);
        search->setIconSize(QSize(22, 22));
        toolbar->addWidget(search);
        homeProviderSelector = new QComboBox(page);
        homeProviderSelector->addItem("Finding providers…", QVariantMap{{"mode", "none"}});
        homeProviderSelector->hide();
        homeProviderButton = button("Finding providers…");
        homeProviderButton->setProperty("providerPicker", true);
        homeProviderButton->setIcon(QIcon(":/icons/source-selector.svg"));
        homeProviderButton->setIconSize(QSize(18, 18));
        homeProviderButton->setMinimumHeight(40);
        homeProviderButton->setMinimumWidth(220);
        homeProviderButton->setMaximumWidth(320);
        homeProviderButton->setEnabled(false);
        homeProviderButton->setToolTip("Discovering installed providers");
        toolbar->addWidget(homeProviderButton);
        auto *profile = new QLabel(settings.value("profileName", "Standard").toString().left(1).toUpper());
        profile->setObjectName("profileAvatar");
        profile->setAlignment(Qt::AlignCenter);
        profile->setFixedSize(40, 40);
        profile->setToolTip(settings.value("profileName", "Standard").toString());
        toolbar->addWidget(profile);
        v->addLayout(toolbar);

        homeSearchPanel = new QWidget(page);
        auto *homeSearchLayout = new QVBoxLayout(homeSearchPanel);
        homeSearchLayout->setContentsMargins(0, 0, 0, 0);
        homeSearchLayout->setSpacing(8);
        homeSearchInput = new QLineEdit(homeSearchPanel);
        homeSearchInput->setObjectName("homeSearchField");
        homeSearchInput->setPlaceholderText("Search this provider…");
        homeSearchInput->setMinimumHeight(46);
        homeSearchInput->setClearButtonEnabled(true);
        homeSearchInput->addAction(QIcon(":/icons/search.svg"), QLineEdit::LeadingPosition);
        homeSearchLayout->addWidget(homeSearchInput);
        homeSearchResults = new QListWidget(homeSearchPanel);
        homeSearchResults->setObjectName("homeSearchResults");
        homeSearchResults->setViewMode(QListView::IconMode);
        homeSearchResults->setResizeMode(QListView::Adjust);
        homeSearchResults->setMovement(QListView::Static);
        homeSearchResults->setUniformItemSizes(true);
        homeSearchResults->setLayoutMode(QListView::Batched);
        homeSearchResults->setBatchSize(12);
        homeSearchResults->setIconSize(CloudStream::ArtworkSizing::posterSize(130));
        homeSearchResults->setGridSize(QSize(146, 246));
        homeSearchResults->setFixedHeight(285);
        homeSearchLayout->addWidget(homeSearchResults);
        homeSearchTypeFilter = new QButtonGroup(this);
        auto *homeSearchAllFilter = button("All");
        homeSearchAllFilter->setCheckable(true);
        homeSearchAllFilter->setChecked(true);
        homeSearchAllFilter->hide();
        homeSearchTypeFilter->addButton(homeSearchAllFilter, 0);
        homeSearchPanel->hide();
        v->addWidget(homeSearchPanel);

        homeHeroBanner = new HomeHeroBanner;
        homeHeroBanner->setObjectName("homeHero");
        homeHeroBanner->setMinimumHeight(330);
        homeHeroBanner->setMaximumHeight(390);
        auto *heroLayout = new QVBoxLayout(homeHeroBanner);
        heroLayout->setContentsMargins(28, 24, 28, 28);
        heroLayout->setSpacing(10);
        heroLayout->addStretch();
        homeHeroTitle = title({}, 30);
        homeHeroTitle->setObjectName("heroTitle");
        homeHeroTitle->setWordWrap(true);
        homeHeroTitle->setMaximumWidth(620);
        heroLayout->addWidget(homeHeroTitle);
        homeHeroMeta = new QLabel;
        homeHeroMeta->setObjectName("heroMeta");
        heroLayout->addWidget(homeHeroMeta);
        auto *heroActions = new QHBoxLayout;
        heroActions->setSpacing(10);
        homeHeroPlay = button("Play", true);
        homeHeroPlay->setIcon(QIcon(":/icons/play-dark.svg"));
        homeHeroInfo = button("Info");
        homeHeroInfo->setIcon(QIcon(":/icons/info.svg"));
        heroActions->addWidget(homeHeroPlay);
        heroActions->addWidget(homeHeroInfo);
        heroActions->addStretch();
        const auto openHero = [this](bool autoPlay) {
            if (!homeHeroUrl.isEmpty() && !homeHeroProvider.isEmpty() && !homeHeroJar.isEmpty()) {
                openDetails(homeHeroJar, homeHeroProvider, homeHeroUrl, autoPlay);
            }
        };
        connect(homeHeroPlay, &QPushButton::clicked, this, [openHero] { openHero(true); });
        connect(homeHeroInfo, &QPushButton::clicked, this, [openHero] { openHero(false); });
        connect(homeProviderSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
            const auto provider = QJsonObject::fromVariantMap(homeProviderSelector->currentData().toMap());
            if (provider.value("mode").toString() == "provider") settings.setValue("homeProviderKey", providerKey(provider));
            else settings.setValue("homeProviderKey", provider.value("mode").toString());
            updateHomeProviderButton();
            if (homeSearchInput && homeSearchInput->isVisible() &&
                !homeSearchInput->text().trimmed().isEmpty()) searchHomeSelectedProvider();
            else loadHomeContent();
        });
        connect(homeProviderButton, &QPushButton::clicked,
                this, &CloudStreamWindow::showHomeProviderDialog);
        heroLayout->addLayout(heroActions);
        v->addWidget(homeHeroBanner);
        homeHeroBanner->hide();

        v->addWidget(continueWatchingSection());

        homeOnboarding = new QWidget;
        homeOnboarding->setObjectName("homeEmptyState");
        homeOnboarding->setMinimumHeight(420);
        homeOnboarding->setMaximumWidth(760);
        homeOnboarding->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto *emptyLayout = new QVBoxLayout(homeOnboarding);
        emptyLayout->setContentsMargins(48, 44, 48, 44);
        emptyLayout->setSpacing(16);
        emptyLayout->addStretch();
        auto *emptyLogo = new QLabel;
        emptyLogo->setPixmap(QPixmap(":/assets/cloudstream-launcher.png").scaled(92, 92, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        emptyLogo->setAlignment(Qt::AlignCenter);
        emptyLogo->setFixedHeight(90);
        emptyLayout->addWidget(emptyLogo);
        homeOnboardingTitle = title("Finding providers…", 23);
        homeOnboardingTitle->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(homeOnboardingTitle);
        homeOnboardingMessage = new QLabel("Checking installed extensions");
        homeOnboardingMessage->setObjectName("emptyStateMessage");
        homeOnboardingMessage->setWordWrap(true);
        homeOnboardingMessage->setAlignment(Qt::AlignCenter);
        homeOnboardingMessage->setMaximumWidth(560);
        homeOnboardingMessage->setMinimumHeight(42);
        emptyLayout->addWidget(homeOnboardingMessage, 0, Qt::AlignHCenter);
        auto *emptyActions = new QHBoxLayout;
        emptyActions->addStretch();
        auto *browseExtensions = button("Browse extensions", true);
        auto *emptyNetwork = button("Network stream");
        emptyActions->addWidget(browseExtensions);
        emptyActions->addWidget(emptyNetwork);
        emptyActions->addStretch();
        emptyLayout->addLayout(emptyActions);
        emptyLayout->addStretch();
        v->addWidget(homeOnboarding, 1, Qt::AlignHCenter);

        connect(search, &QToolButton::clicked, this, [this, search] {
            homeSearchOpen = !homeSearchOpen;
            homeSearchPanel->setVisible(homeSearchOpen);
            search->setToolTip(homeSearchOpen ? "Close Home search" : "Search this Home provider");
            if (homeSearchOpen) {
                homeSearchInput->setFocus(Qt::ShortcutFocusReason);
            } else {
                cancelHomeSearch();
                homeSearchResults->clear();
            }
        });
        connect(homeSearchInput, &QLineEdit::returnPressed,
                this, &CloudStreamWindow::searchHomeSelectedProvider);
        connect(homeSearchResults, &QListWidget::itemActivated, this,
                [this](QListWidgetItem *item) {
            const auto url = item->data(Qt::UserRole).toString();
            const auto provider = item->data(Qt::UserRole + 1).toString();
            const auto jar = item->data(Qt::UserRole + 2).toString();
            if (!url.isEmpty() && !provider.isEmpty() && !jar.isEmpty())
                openDetails(jar, provider, url);
        });
        connect(emptyNetwork, &QPushButton::clicked, this, [this] { showNetworkStreamDialog(); });
        connect(browseExtensions, &QPushButton::clicked, this, [this] { pages->setCurrentIndex(4); });

        homeSectionsContainer = new QWidget;
        homeSectionsContainer->hide();
        homeSectionsLayout = new QVBoxLayout(homeSectionsContainer);
        homeSectionsLayout->setContentsMargins(0, 0, 0, 0);
        homeSectionsLayout->setSpacing(18);
        v->addWidget(homeSectionsContainer);
        homeScrollArea = qobject_cast<QScrollArea *>(scrollPage(page));
        auto *homeScrollBar = homeScrollArea->verticalScrollBar();
        connect(homeScrollBar, &QScrollBar::valueChanged, this,
                [this](int) { maybeAppendHomeSections(); });
        connect(homeScrollBar, &QScrollBar::rangeChanged, this,
                [this](int, int) { maybeAppendHomeSections(); });
        return homeScrollArea;
    }

    QWidget *continueWatchingSection() {
        auto *section = new QWidget;
        continueWatchingCard = section;
        section->setObjectName("mediaSection");
        auto *layout = new QVBoxLayout(section);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *header = new QHBoxLayout;
        header->addWidget(title("Continue watching", 18));
        header->addStretch();
        auto *browse = button("Library");
        browse->setMaximumWidth(100);
        connect(browse, &QPushButton::clicked, this, [this] { pages->setCurrentIndex(2); });
        header->addWidget(browse);
        layout->addLayout(header);
        continueWatchingList = new QListWidget;
        continueWatchingList->setProperty("smoothHorizontalWheel", true);
        continueWatchingList->setFocusPolicy(Qt::StrongFocus);
        continueWatchingList->setViewMode(QListView::IconMode);
        continueWatchingList->setFlow(QListView::LeftToRight);
        continueWatchingList->setWrapping(false);
        continueWatchingList->setMovement(QListView::Static);
        continueWatchingList->setUniformItemSizes(true);
        continueWatchingList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        continueWatchingList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        continueWatchingList->setIconSize(CloudStream::ArtworkSizing::posterSize(150));
        continueWatchingList->setGridSize(QSize(166, 280));
        continueWatchingList->setFixedHeight(292);
        continueWatchingList->setObjectName("posterRow");
        continueWatchingList->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(continueWatchingList, &QListWidget::customContextMenuRequested,
                this, [this](const QPoint &position) {
            auto *item = continueWatchingList->itemAt(position);
            if (!item) return;
            const auto id = item->data(Qt::UserRole).toString();
            if (id.isEmpty()) return;
            QMenu menu(continueWatchingList);
            auto *remove = menu.addAction("Remove from Continue watching");
            if (menu.exec(continueWatchingList->viewport()->mapToGlobal(position)) != remove) return;
            if (history.remove(id)) {
                refreshContinueWatching();
                if (libraryRefresh) libraryRefresh();
            }
        });
        connect(continueWatchingList, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            const auto id = item->data(Qt::UserRole).toString();
            if (id.isEmpty()) return;
            for (const auto &entry : history.entries()) {
                if (entry.id != id) continue;
                if (!entry.playbackData.isEmpty() && !entry.provider.isEmpty() && !entry.jarPath.isEmpty()) {
                    const auto playerTitle = entry.episodeName.isEmpty()
                        ? entry.name : entry.name + " • " + entry.episodeName;
                    resolveAndPlay(entry.jarPath, entry.provider, entry.playbackData, entry.id, playerTitle);
                } else if (!entry.sourceUrl.isEmpty() && !entry.provider.isEmpty() && !entry.jarPath.isEmpty()) {
                    openDetails(entry.jarPath, entry.provider, entry.sourceUrl);
                }
                break;
            }
        });
        layout->addWidget(continueWatchingList);
        return section;
    }

    void refreshContinueWatching() {
        if (!continueWatchingList) return;
        continueWatchingList->clear();
        const auto entries = history.entries("Watching");
        if (continueWatchingCard) continueWatchingCard->setVisible(!entries.isEmpty());
        if (entries.isEmpty()) {
            return;
        }
        const auto coverSize = CloudStream::ArtworkSizing::posterSize(150);
        QPixmap placeholder(coverSize);
        placeholder.fill(QColor("#282a35"));
        {
            QPainter painter(&placeholder);
            const auto logo = QPixmap(":/assets/cloudstream.svg").scaled(76, 76, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            painter.setOpacity(0.35);
            painter.drawPixmap((placeholder.width() - logo.width()) / 2, (placeholder.height() - logo.height()) / 2, logo);
        }
        const auto count = std::min<qsizetype>(entries.size(), 20);
        for (int index = 0; index < count; ++index) {
            const auto &entry = entries[index];
            QStringList lines{entry.name};
            if (!entry.episodeName.isEmpty()) lines << entry.episodeName;
            if (entry.durationSeconds > 0.0) {
                const auto percent = qBound(0, qRound((entry.positionSeconds / entry.durationSeconds) * 100.0), 100);
                lines << QString::number(percent) + "% watched";
            } else {
                lines << "Ready to play";
            }
            auto *item = new QListWidgetItem(QIcon(placeholder), lines.join('\n'));
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            item->setData(Qt::UserRole, entry.id);
            item->setToolTip(entry.name + (entry.episodeName.isEmpty() ? QString() : " • " + entry.episodeName));
            continueWatchingList->addItem(item);
            if (entry.posterUrl.isEmpty()) continue;
            const QPointer<QListWidget> safeList(continueWatchingList);
            const QPersistentModelIndex itemIndex(
                continueWatchingList->model()->index(continueWatchingList->row(item), 0));
            artworkLoader->load(QUrl(entry.posterUrl), coverSize, continueWatchingList,
                [safeList, itemIndex](const QImage &image) {
                    if (!safeList || !itemIndex.isValid()) return;
                    if (auto *liveItem = safeList->item(itemIndex.row())) {
                        liveItem->setIcon(QIcon(QPixmap::fromImage(image)));
                    }
                }, CloudStream::ArtworkLoader::HighPriority);
        }
    }

    void setHomeHeroEmpty(const QString &heading, const QString &message) {
        homeHeroUrl.clear();
        homeHeroProvider.clear();
        homeHeroJar.clear();
        homeHeroArtworkUrl.clear();
        if (homeHeroBanner) {
            homeHeroBanner->setBackdrop({});
            homeHeroBanner->hide();
        }
        if (homeOnboarding) homeOnboarding->show();
        if (homeOnboardingTitle) homeOnboardingTitle->setText(heading);
        if (homeOnboardingMessage) homeOnboardingMessage->setText(message);
        if (homeSectionsContainer) homeSectionsContainer->hide();
        if (homeHeroTitle) homeHeroTitle->setText(heading);
        if (homeHeroMeta) homeHeroMeta->setText(message);
        if (homeHeroPlay) homeHeroPlay->setEnabled(false);
        if (homeHeroInfo) homeHeroInfo->setEnabled(false);
    }

    void updateHomeHero(const QJsonArray &sections, const QString &providerLabel) {
        auto providerName = providerLabel;
        if (providerName.startsWith("Cached ")) providerName.remove(0, 7);
        const auto hero = CloudStream::HomeHeroSelection::select(sections, providerName);
        if (!hero.valid) {
            setHomeHeroEmpty(providerName, "This provider returned no featured title");
            return;
        }
        homeHeroUrl = hero.url;
        homeHeroProvider = hero.providerName;
        homeHeroJar = currentHomeJar;
        homeHeroArtworkUrl = hero.posterUrl;
        if (homeOnboarding) homeOnboarding->hide();
        if (homeHeroBanner) homeHeroBanner->show();
        if (homeSectionsContainer) homeSectionsContainer->show();
        if (homeHeroTitle) homeHeroTitle->setText(hero.name);
        if (homeHeroMeta) homeHeroMeta->setText(hero.providerName + "  •  Featured on Home");
        if (homeHeroPlay) homeHeroPlay->setEnabled(true);
        if (homeHeroInfo) homeHeroInfo->setEnabled(true);
        if (homeHeroBanner) homeHeroBanner->setBackdrop({});
        if (hero.posterUrl.isEmpty()) return;
        const auto artworkUrl = hero.posterUrl;
        const QPointer<HomeHeroBanner> safeHero(homeHeroBanner);
        artworkLoader->load(QUrl(artworkUrl), QSize(1120, 1680), homeHeroBanner,
            [this, safeHero, artworkUrl](const QImage &image) {
            if (safeHero && artworkUrl == homeHeroArtworkUrl) {
                safeHero->setBackdrop(QPixmap::fromImage(image));
            }
        }, CloudStream::ArtworkLoader::HighPriority,
           CloudStream::ArtworkLoader::FitInside);
    }

    void clearHomeSections() {
        if (!homeSectionsLayout) return;
        progressiveHomeSections = {};
        progressiveHomeProviderName.clear();
        renderedHomeSectionCount = 0;
        longestHomeAppendMs = 0;
        ++homeSectionGeneration;
        homeAppendScheduled = false;
        while (auto *item = homeSectionsLayout->takeAt(0)) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }

    void resetHomeViewport() {
        if (!homeScrollArea) return;
        if (!navigationButtons.isEmpty()) navigationButtons.first()->setFocus(Qt::OtherFocusReason);
        homeScrollArea->verticalScrollBar()->setValue(0);
        QTimer::singleShot(0, homeScrollArea, [this] { homeScrollArea->verticalScrollBar()->setValue(0); });
        QTimer::singleShot(150, homeScrollArea, [this] { homeScrollArea->verticalScrollBar()->setValue(0); });
    }

    void showHomeMessage(const QString &message) {
        clearHomeSections();
        homeContentRendered = false;
        auto *label = new QLabel(message);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumHeight(130);
        label->setStyleSheet("color:#9395a3;background:transparent;padding:28px;");
        homeSectionsLayout->addWidget(label);
        if (resetHomeScrollPending && homeScrollArea) {
            resetHomeScrollPending = false;
            resetHomeViewport();
        }
    }

    QWidget *providerHomeSection(const QJsonObject &section, int distanceFromViewport) {
        auto *card = new QWidget;
        card->setObjectName("mediaSection");
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto *header = new QHBoxLayout;
        header->addWidget(title(section.value("name").toString("Featured"), 19));
        header->addStretch();
        auto *previous = new QToolButton;
        previous->setText("‹");
        previous->setAccessibleName("Previous titles");
        previous->setFixedSize(34, 34);
        auto *next = new QToolButton;
        next->setText("›");
        next->setAccessibleName("Next titles");
        next->setFixedSize(34, 34);
        header->addWidget(previous);
        header->addWidget(next);
        layout->addLayout(header);

        auto *list = new QListWidget;
        list->setProperty("smoothHorizontalWheel", true);
        const bool horizontal = section.value("horizontalImages").toBool();
        const QSize iconSize = horizontal
            ? CloudStream::ArtworkSizing::backdropSize(240)
            : CloudStream::ArtworkSizing::posterSize(150);
        const QSize gridSize = horizontal ? QSize(254, 184) : QSize(166, 280);
        list->setFocusPolicy(Qt::StrongFocus);
        list->setViewMode(QListView::IconMode);
        list->setFlow(QListView::LeftToRight);
        list->setWrapping(false);
        list->setResizeMode(QListView::Adjust);
        list->setMovement(QListView::Static);
        list->setUniformItemSizes(true);
        list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setIconSize(iconSize);
        list->setGridSize(gridSize);
        list->setFixedHeight(gridSize.height() + 12);
        list->setObjectName("posterRow");
        CloudStream::SmoothScrollController::attach(
            list, CloudStream::SmoothScrollController::HorizontalWheel);
        auto *horizontalScroll = list->horizontalScrollBar();
        connect(previous, &QToolButton::clicked, list, [list, gridSize] {
            CloudStream::SmoothScrollController::scrollBy(list, -gridSize.width() * 3, 0);
        });
        connect(next, &QToolButton::clicked, list, [list, gridSize] {
            CloudStream::SmoothScrollController::scrollBy(list, gridSize.width() * 3, 0);
        });
        auto refreshArrows = [horizontalScroll, previous, next] {
            previous->setEnabled(horizontalScroll->value() > horizontalScroll->minimum());
            next->setEnabled(horizontalScroll->value() < horizontalScroll->maximum());
        };
        connect(horizontalScroll, &QScrollBar::valueChanged, list, [refreshArrows](int) { refreshArrows(); });
        connect(horizontalScroll, &QScrollBar::rangeChanged, list, [refreshArrows](int, int) { refreshArrows(); });
        QTimer::singleShot(0, list, refreshArrows);

        QPixmap placeholder(iconSize);
        placeholder.fill(QColor("#282a35"));
        {
            QPainter painter(&placeholder);
            const auto logo = QPixmap(":/assets/cloudstream.svg").scaled(iconSize.width() / 2, iconSize.height() / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            painter.setOpacity(0.35);
            painter.drawPixmap((placeholder.width() - logo.width()) / 2, (placeholder.height() - logo.height()) / 2, logo);
        }

        list->setUpdatesEnabled(false);
        int posterIndex = 0;
        for (const auto &value : section.value("items").toArray()) {
            const auto media = value.toObject();
            auto *item = new QListWidgetItem(QIcon(placeholder), media.value("name").toString("Untitled"));
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            item->setData(Qt::UserRole, media.value("url").toString());
            item->setData(Qt::UserRole + 1, media.value("apiName").toString());
            item->setData(Qt::UserRole + 2, currentHomeJar);
            item->setToolTip(media.value("name").toString());
            list->addItem(item);
            const auto posterUrl = media.value("posterUrl").toString();
            if (!posterUrl.isEmpty()) {
                const QPointer<QListWidget> safeList(list);
                const QPersistentModelIndex itemIndex(
                    list->model()->index(list->row(item), 0));
                const auto priority = distanceFromViewport == 0 && posterIndex < 8
                    ? CloudStream::ArtworkLoader::HighPriority
                    : (distanceFromViewport < 2
                           ? CloudStream::ArtworkLoader::NormalPriority
                           : CloudStream::ArtworkLoader::LowPriority);
                artworkLoader->load(QUrl(posterUrl), iconSize, list,
                    [safeList, itemIndex](const QImage &image) {
                    if (!safeList || !itemIndex.isValid()) return;
                    if (auto *liveItem = safeList->item(itemIndex.row())) {
                        liveItem->setIcon(QIcon(QPixmap::fromImage(image)));
                    }
                }, priority);
            }
            ++posterIndex;
        }
        list->setUpdatesEnabled(true);
        list->horizontalScrollBar()->setValue(0);
        connect(list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            const auto url = item->data(Qt::UserRole).toString();
            const auto provider = item->data(Qt::UserRole + 1).toString();
            const auto jar = item->data(Qt::UserRole + 2).toString();
            if (!url.isEmpty() && !provider.isEmpty() && !jar.isEmpty()) openDetails(jar, provider, url);
        });
        layout->addWidget(list);
        return card;
    }

    void appendHomeSections(int batchSize) {
        if (!homeSectionsLayout || homeAppendingSections || progressiveHomeSections.isEmpty()) return;
        const auto nextCount = CloudStream::HomeContentLimiter::nextSectionCount(
            renderedHomeSectionCount, progressiveHomeSections.size(), batchSize);
        if (nextCount <= renderedHomeSectionCount) return;
        QElapsedTimer batchTimer;
        batchTimer.start();
        homeAppendingSections = true;
        if (homeSectionsContainer) homeSectionsContainer->setUpdatesEnabled(false);
        const auto firstSection = renderedHomeSectionCount;
        const auto limited = CloudStream::HomeContentLimiter::limitRange(
            progressiveHomeSections, firstSection, nextCount - firstSection, 24);
        for (int offset = 0; offset < limited.size(); ++offset) {
            homeSectionsLayout->addWidget(
                providerHomeSection(limited[offset].toObject(), offset));
        }
        renderedHomeSectionCount = nextCount;
        if (homeSectionsContainer) homeSectionsContainer->setUpdatesEnabled(true);
        longestHomeAppendMs = std::max(longestHomeAppendMs, batchTimer.elapsed());
        homeAppendingSections = false;
        homeContentRendered = renderedHomeSectionCount > 0;
        qInfo().noquote() << progressiveHomeProviderName << ": showing"
                          << renderedHomeSectionCount << "of"
                          << progressiveHomeSections.size() << "Home section(s)";
    }

    void maybeAppendHomeSections() {
        if (!homeScrollArea || homeAppendScheduled || homeAppendingSections ||
            renderedHomeSectionCount >= progressiveHomeSections.size()) return;
        const auto *bar = homeScrollArea->verticalScrollBar();
        if (bar->maximum() > 0 &&
            bar->value() + (bar->pageStep() * 2) < bar->maximum()) return;
        homeAppendScheduled = true;
        const auto generation = homeSectionGeneration;
        QTimer::singleShot(0, homeSectionsContainer, [this, generation] {
            if (generation != homeSectionGeneration) return;
            homeAppendScheduled = false;
            appendHomeSections(4);
        });
    }

    void renderHomeSections(const QJsonArray &sections, const QString &providerName) {
        clearHomeSections();
        progressiveHomeSections = sections;
        progressiveHomeProviderName = providerName;
        updateHomeHero(sections, providerName);
        appendHomeSections(6);
        if (resetHomeScrollPending && homeScrollArea) {
            resetHomeScrollPending = false;
            resetHomeViewport();
        }
        maybeAppendHomeSections();
    }

    void loadHomeContent() {
        if (!homeSectionsLayout) return;
        const auto homeGeneration = homeRequestGeneration.begin();
        if (activeHomeProcess && activeHomeProcess->state() != QProcess::NotRunning) {
            activeHomeProcess->kill();
        }
        const auto mode = homeProviderSelector
            ? QJsonObject::fromVariantMap(homeProviderSelector->currentData().toMap()).value("mode").toString()
            : QString();
        if (mode == "none") {
            resetHomeScrollPending = true;
            currentHomeProviderKey.clear();
            setHomeHeroEmpty("Choose a provider", providerChoices.isEmpty()
                ? "Install an extension in Settings to populate Home"
                : "Select a provider from the pill above");
            showHomeMessage(providerChoices.isEmpty()
                ? "No enabled provider is installed yet. Download an extension in Settings; Android .cs3 packages will be converted and validated automatically."
                : "Home is disabled. Choose a provider or Random above.");
            return;
        }
        const auto provider = selectedProvider();
        const auto jar = provider.value("jarPath").toString();
        const auto providerName = provider.value("name").toString();
        if (jar.isEmpty() || providerName.isEmpty()) {
            currentHomeProviderKey.clear();
            setHomeHeroEmpty("No Home provider", "Install or enable a compatible extension in Settings");
            showHomeMessage("No runnable provider is available. Install and enable a Linux/JVM extension in Settings.");
            return;
        }
        if (currentHomeProviderKey != providerKey(provider)) resetHomeScrollPending = true;
        currentHomeJar = jar;
        currentHomeProviderKey = providerKey(provider);
        setHomeHeroEmpty("Loading " + providerName + "…", providerName);
        const auto requestKey = currentHomeProviderKey;
        homeContentRendered = false;
        const auto providerHash = QString::fromLatin1(
            QCryptographicHash::hash(requestKey.toUtf8(), QCryptographicHash::Sha256).toHex());
        const auto cachePath = CloudStream::XdgPaths::cacheDir() + "/provider-home-" + providerHash + ".json";
        QJsonArray cachedSections;
        QFile cache(cachePath);
        if (cache.open(QIODevice::ReadOnly)) {
            cachedSections = QJsonDocument::fromJson(cache.readAll()).array();
            if (!cachedSections.isEmpty()) renderHomeSections(cachedSections, "Cached " + providerName);
        }
        const auto helper = CloudStream::ProviderHostCommand::discover();
        if (helper.isEmpty()) {
            showHomeMessage("The provider host is missing. Rebuild CloudStream Linux.");
            return;
        }
        if (!homeContentRendered) showHomeMessage("Loading provider home…");
        auto *homeProcess = new QProcess(this);
        activeHomeProcess = homeProcess;
        helper.configure(homeProcess, {"home", jar, "auto", providerName});
        CloudStream::ProcessCompletion::watch(homeProcess, this,
                [this, homeProcess, providerName, cachePath, requestKey, cachedSections, homeGeneration]
                (int homeExit, QProcess::ExitStatus exitStatus, bool startFailure) {
            if (activeHomeProcess == homeProcess) activeHomeProcess = nullptr;
            if (!homeRequestGeneration.isCurrent(homeGeneration) ||
                requestKey != currentHomeProviderKey) {
                homeProcess->deleteLater();
                return;
            }
            const auto result = CloudStream::HomeProcessResult::parse(
                homeProcess->readAllStandardOutput(), homeProcess->readAllStandardError(),
                homeExit, exitStatus, startFailure, homeProcess->property("homeTimedOut").toBool(),
                homeProcess->errorString());
            const auto sections = result.sections;
            if (!result.error.isEmpty() || sections.isEmpty()) {
                if (!homeContentRendered) {
                    setHomeHeroEmpty(providerName, result.error.isEmpty()
                        ? "No Home sections available" : "Could not load Home");
                    showHomeMessage(!result.error.isEmpty()
                        ? result.error
                        : providerName + " returned no Home sections. Use Search or choose another provider.");
                }
                else status->setText(result.error.isEmpty()
                    ? "Provider refresh returned no sections; showing cached Home"
                    : "Showing cached Home. " + result.error);
                homeProcess->deleteLater();
                return;
            }
            QSaveFile cacheFile(cachePath);
            if (cacheFile.open(QIODevice::WriteOnly)) {
                cacheFile.write(QJsonDocument(sections).toJson(QJsonDocument::Compact));
                cacheFile.commit();
            }
            if (homeContentRendered &&
                CloudStream::HomeContentLimiter::equivalent(cachedSections, sections)) {
                status->setText(providerName + " Home is up to date");
                homeProcess->deleteLater();
                return;
            }
            renderHomeSections(sections, providerName);
            homeProcess->deleteLater();
        });
        homeProcess->start();
        QTimer::singleShot(20000, homeProcess, [homeProcess] {
            if (homeProcess->state() != QProcess::NotRunning) {
                homeProcess->setProperty("homeTimedOut", true);
                homeProcess->kill();
            }
        });
    }

    void cancelHomeSearch() {
        ++homeSearchGeneration;
        if (activeHomeSearchProcess && activeHomeSearchProcess->state() != QProcess::NotRunning) {
            activeHomeSearchProcess->kill();
        }
        activeHomeSearchProcess = nullptr;
        if (homeSearchArtworkContext) homeSearchArtworkContext->deleteLater();
        homeSearchArtworkContext = nullptr;
    }

    void searchHomeSelectedProvider() {
        if (!homeSearchInput || !homeSearchResults) return;
        const auto term = homeSearchInput->text().trimmed();
        cancelHomeSearch();
        homeSearchResults->clear();
        if (term.isEmpty()) {
            homeSearchResults->hide();
            return;
        }
        const auto provider = selectedProvider();
        const auto jar = provider.value("jarPath").toString();
        const auto providerName = provider.value("name").toString();
        if (jar.isEmpty() || providerName.isEmpty()) {
            homeSearchResults->addItem("Choose a Home provider first.");
            homeSearchResults->show();
            status->setText("No Home provider selected");
            return;
        }
        const auto helper = CloudStream::ProviderHostCommand::discover();
        if (helper.isEmpty()) {
            homeSearchResults->addItem("Provider host is not built. Run linux-native/build.sh.");
            homeSearchResults->show();
            status->setText("Provider host missing");
            return;
        }
        homeSearchResults->show();
        status->setText("Searching " + providerName + "");
        const auto generation = ++homeSearchGeneration;
        homeSearchArtworkContext = new QObject(homeSearchResults);
        const QPointer<QObject> artworkContext(homeSearchArtworkContext);
        auto *process = new QProcess(this);
        activeHomeSearchProcess = process;
        helper.configure(process, {"search", jar, "auto", providerName, term});
        CloudStream::ProcessCompletion::watch(process, this,
            [this, process, generation, providerName, jar, artworkContext]
            (int exitCode, QProcess::ExitStatus, bool startFailure) {
            if (generation != homeSearchGeneration) {
                process->deleteLater();
                return;
            }
            if (activeHomeSearchProcess == process) activeHomeSearchProcess = nullptr;
            QList<QJsonObject> values;
            if (!startFailure && exitCode == 0) {
                const auto array = QJsonDocument::fromJson(
                    process->readAllStandardOutput()).array();
                for (const auto &value : array) {
                    auto result = value.toObject();
                    result.insert("_jarPath", jar);
                    result.insert("_providerName", providerName);
                    values.append(result);
                }
            }
            if (homeSearchResults) {
                homeSearchResults->clear();
                QSet<QString> seen;
                const auto added = artworkContext
                    ? appendSearchResults(homeSearchResults, homeSearchTypeFilter,
                                          values, &seen, artworkContext)
                    : 0;
                if (added == 0) homeSearchResults->addItem(
                    startFailure ? "Could not start the provider host."
                                 : (exitCode == 0 ? "No results found" : providerName + " search failed."));
            }
            status->setText(QString::number(values.size()) + " result(s) from " + providerName);
            process->deleteLater();
        });
        process->start();
        QTimer::singleShot(20000, process, [process] {
            if (process->state() != QProcess::NotRunning) process->kill();
        });
    }

    static bool searchTypeMatches(const QString &type, int filter) {
        return filter == 0 ||
            (filter == 1 && type == "Movie") ||
            (filter == 2 && type.contains("Tv", Qt::CaseInsensitive)) ||
            (filter == 3 && (type.contains("Anime", Qt::CaseInsensitive) || type == "OVA")) ||
            (filter == 4 && type.contains("Live", Qt::CaseInsensitive));
    }

    void applySearchFilters(QListWidget *results, QButtonGroup *typeFilter,
                            QComboBox *providerFilter = nullptr) {
        if (!results || !typeFilter) return;
        const auto filter = typeFilter->checkedId();
        const auto provider = providerFilter ? providerFilter->currentData().toString() : QString();
        int visible = 0;
        results->setUpdatesEnabled(false);
        for (int row = 0; row < results->count(); ++row) {
            auto *item = results->item(row);
            const auto matches = searchTypeMatches(item->data(Qt::UserRole + 4).toString(), filter) &&
                (provider.isEmpty() || item->data(Qt::UserRole + 1).toString() == provider);
            item->setHidden(!matches);
            if (matches) ++visible;
        }
        results->setUpdatesEnabled(true);
        if (status) status->setText(QString::number(visible) + " visible search result(s)");
    }

    void applySearchTypeFilter(QListWidget *results, QButtonGroup *typeFilter) {
        applySearchFilters(results, typeFilter, searchResultProviderFilter);
    }

    int appendSearchResults(QListWidget *results, QButtonGroup *typeFilter,
                            const QList<QJsonObject> &values, QSet<QString> *seen,
                            QObject *artworkContext, QComboBox *providerFilter = nullptr) {
        if (!results || !typeFilter || !seen || !artworkContext) return 0;
        QPixmap placeholder(CloudStream::ArtworkSizing::posterSize(150));
        placeholder.fill(QColor("#282a35"));
        int added = 0;
        results->setUpdatesEnabled(false);
        for (const auto &result : values) {
            const auto type = result.value("type").toString();
            const auto jar = result.value("_jarPath").toString();
            const auto apiName = result.value("apiName").toString(result.value("_providerName").toString());
            const auto resultUrl = result.value("url").toString();
            const auto identity = jar + "\n" + apiName + "\n" + resultUrl;
            if (seen->contains(identity)) continue;
            seen->insert(identity);
            auto *item = new QListWidgetItem(QIcon(placeholder), result.value("name").toString());
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            item->setData(Qt::UserRole, resultUrl);
            item->setData(Qt::UserRole + 1, apiName);
            item->setData(Qt::UserRole + 2, jar);
            item->setData(Qt::UserRole + 3, result.value("posterUrl").toString());
            item->setData(Qt::UserRole + 4, type);
            item->setData(Qt::UserRole + 5, identity);
            item->setToolTip(result.value("name").toString() + " • " + type + " • " + apiName);
            results->addItem(item);
            const auto provider = providerFilter ? providerFilter->currentData().toString() : QString();
            item->setHidden(!searchTypeMatches(type, typeFilter->checkedId()) ||
                            (!provider.isEmpty() && apiName != provider));
            ++added;
            const auto posterUrl = result.value("posterUrl").toString();
            if (posterUrl.isEmpty()) continue;
            const QPointer<QListWidget> safeResults(results);
            const QPersistentModelIndex itemIndex(
                results->model()->index(results->row(item), 0));
            const auto priority = results->count() <= 18
                ? CloudStream::ArtworkLoader::HighPriority
                : (results->count() <= 48
                       ? CloudStream::ArtworkLoader::NormalPriority
                       : CloudStream::ArtworkLoader::LowPriority);
            artworkLoader->load(QUrl(posterUrl), CloudStream::ArtworkSizing::posterSize(150),
                artworkContext, [safeResults, itemIndex](const QImage &image) {
                if (!safeResults || !itemIndex.isValid()) return;
                if (auto *liveItem = safeResults->item(itemIndex.row())) {
                    liveItem->setIcon(QIcon(QPixmap::fromImage(image)));
                }
            }, priority);
        }
        results->setUpdatesEnabled(true);
        return added;
    }

    void cancelActiveSearch() {
        searchRequestGeneration.begin();
        if (searchArtworkContext) searchArtworkContext->deleteLater();
        searchArtworkContext = nullptr;
        for (const auto &safeProcess : activeSearchProcesses) {
            if (safeProcess && safeProcess->state() != QProcess::NotRunning) {
                safeProcess->kill();
            }
        }
        activeSearchProcesses.clear();
    }

    QWidget *searchPage() {
        auto *page = pageFrame("Search", {});
        auto *v = qobject_cast<QVBoxLayout *>(page->layout());
        auto *query = new QLineEdit;
        searchInput = query;
        query->setObjectName("searchField");
        query->setPlaceholderText("Search…");
        query->setMinimumHeight(52);
        query->setClearButtonEnabled(true);
        auto *submitAction = query->addAction(QIcon(":/icons/search.svg"), QLineEdit::LeadingPosition);
        auto *providerAction = query->addAction(QIcon(":/icons/tune.svg"), QLineEdit::TrailingPosition);
        submitAction->setToolTip("Search");
        providerAction->setToolTip("Choose search providers");
        v->addWidget(query);

        auto *chipRow = new QHBoxLayout;
        chipRow->setSpacing(8);
        auto *typeFilter = new QButtonGroup(this);
        const QStringList types = {"All", "Movies", "TV series", "Anime", "Livestreams"};
        for (int i = 0; i < types.size(); ++i) {
            auto *chip = button(types[i]);
            chip->setCheckable(true);
            chip->setProperty("chip", true);
            typeFilter->addButton(chip, i);
            chipRow->addWidget(chip);
            if (i == 0) chip->setChecked(true);
        }
        typeFilter->setExclusive(true);
        searchTypeFilter = typeFilter;
        searchProviderButton = button("Providers: finding…");
        searchProviderButton->setProperty("chip", true);
        searchProviderButton->setEnabled(false);
        chipRow->addWidget(searchProviderButton);
        searchResultProviderFilter = new QComboBox;
        searchResultProviderFilter->setObjectName("searchResultProviderFilter");
        searchResultProviderFilter->setProperty("chip", true);
        searchResultProviderFilter->addItem("All providers", "");
        searchResultProviderFilter->setToolTip("Filter results by provider");
        chipRow->addWidget(searchResultProviderFilter);
        chipRow->addStretch();
        v->addLayout(chipRow);

        auto *historyPanel = new QWidget;
        historyPanel->setObjectName("searchHistory");
        auto *historyLayout = new QVBoxLayout(historyPanel);
        historyLayout->setContentsMargins(0, 10, 0, 0);
        historyLayout->setSpacing(8);
        auto *historyHeader = new QHBoxLayout;
        historyHeader->addWidget(title("Recent searches", 18));
        historyHeader->addStretch();
        auto *clearHistory = button("Clear history");
        clearHistory->setIcon(QIcon(":/icons/delete.svg"));
        historyHeader->addWidget(clearHistory);
        historyLayout->addLayout(historyHeader);
        auto *historyList = new QListWidget;
        historyList->setObjectName("historyList");
        historyList->setMaximumHeight(260);
        historyLayout->addWidget(historyList);
        v->addWidget(historyPanel);

        auto *searchEmpty = new QWidget;
        searchEmpty->setObjectName("searchEmptyState");
        auto *emptyLayout = new QVBoxLayout(searchEmpty);
        emptyLayout->setContentsMargins(0, 30, 0, 70);
        emptyLayout->setSpacing(12);
        emptyLayout->addStretch();
        auto *emptyIcon = new QLabel;
        emptyIcon->setPixmap(QIcon(":/icons/search.svg").pixmap(QSize(46, 46)));
        emptyIcon->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyIcon);
        auto *emptyTitle = title("Find something to watch", 20);
        emptyTitle->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyTitle);
        auto *emptyMessage = new QLabel("Search every selected provider at once");
        emptyMessage->setObjectName("muted");
        emptyMessage->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyMessage);
        emptyLayout->addStretch();
        v->addWidget(searchEmpty, 1);

        auto *results = new QListWidget;
        searchResultsList = results;
        results->setObjectName("mediaList");
        results->setViewMode(QListView::IconMode);
        results->setResizeMode(QListView::Adjust);
        results->setMovement(QListView::Static);
        results->setUniformItemSizes(true);
        results->setLayoutMode(QListView::Batched);
        results->setBatchSize(24);
        results->setIconSize(CloudStream::ArtworkSizing::posterSize(150));
        results->setGridSize(QSize(166, 280));
        results->setSpacing(8);
        results->hide();
        v->addWidget(results, 1);

        auto refreshHistory = [this, historyList, historyPanel, searchEmpty] {
            historyList->clear();
            const auto values = settings.value("searchHistory").toStringList();
            for (const auto &value : values) {
                auto *item = new QListWidgetItem(QIcon(":/icons/search.svg"), value);
                item->setData(Qt::UserRole, value);
                historyList->addItem(item);
            }
            historyPanel->setVisible(!values.isEmpty());
            searchEmpty->setVisible(values.isEmpty());
        };
        refreshHistory();

        auto search = std::make_shared<std::function<void()>>();
        *search = [this, query, results, historyPanel, searchEmpty, typeFilter, refreshHistory] {
            const auto term = query->text().trimmed();
            if (term.isEmpty()) return;
            cancelActiveSearch();
            const auto providers = selectedSearchProviders();
            if (providers.isEmpty()) {
                results->clear();
                results->addItem("Choose at least one Search provider.");
                status->setText("No Search provider selected");
                return;
            }
            settings.setValue("searchHistory", CloudStream::SearchHistoryModel::add(
                settings.value("searchHistory").toStringList(), term));
            refreshHistory();
            historyPanel->hide();
            searchEmpty->hide();
            results->show();
            const auto helper = CloudStream::ProviderHostCommand::discover();
            if (helper.isEmpty()) {
                results->clear(); results->addItem("Provider host is not built. Run linux-native/build.sh."); status->setText("Provider host missing"); return;
            }
            const auto generation = searchRequestGeneration.begin();
            searchPeakConcurrentProcesses = 0;
            searchArtworkContext = new QObject(results);
            const QPointer<QObject> artworkContext(searchArtworkContext);
            results->clear();
            status->setText("Searching " + QString::number(providers.size()) + " provider(s)");
            struct SearchRun {
                QList<QJsonObject> providers;
                QSet<QString> seen;
                int nextProvider = 0;
                int active = 0;
                int remaining = 0;
                int failures = 0;
                std::function<void()> launchMore;
            };
            auto run = std::make_shared<SearchRun>();
            run->providers = providers;
            run->remaining = providers.size();
            run->launchMore = [this, run, helper, term, results, typeFilter,
                               generation, artworkContext] {
                if (!searchRequestGeneration.isCurrent(generation)) {
                    run->launchMore = {};
                    return;
                }
                constexpr int maximumConcurrentProviders = 4;
                while (run->active < maximumConcurrentProviders &&
                       run->nextProvider < run->providers.size()) {
                    const auto provider = run->providers[run->nextProvider++];
                    const auto jar = provider.value("jarPath").toString();
                    const auto providerName = provider.value("name").toString();
                    auto *searchProcess = new QProcess(this);
                    activeSearchProcesses.append(searchProcess);
                    ++run->active;
                    searchPeakConcurrentProcesses = std::max(
                        searchPeakConcurrentProcesses, run->active);
                    helper.configure(searchProcess, {"search", jar, "auto", providerName, term});
                    auto handled = std::make_shared<bool>(false);
                    const auto finishProvider =
                        [this, run, searchProcess, results, typeFilter, jar,
                         providerName, generation, artworkContext, handled]
                        (int searchExit, bool failedToStart) {
                        if (*handled) return;
                        *handled = true;
                        activeSearchProcesses.removeAll(searchProcess);
                        if (!searchRequestGeneration.isCurrent(generation)) {
                            searchProcess->deleteLater();
                            run->launchMore = {};
                            return;
                        }
                        QList<QJsonObject> batch;
                        if (!failedToStart && searchExit == 0) {
                            const auto values = QJsonDocument::fromJson(
                                searchProcess->readAllStandardOutput()).array();
                            for (const auto &value : values) {
                                auto result = value.toObject();
                                result.insert("_jarPath", jar);
                                result.insert("_providerName", providerName);
                                batch.append(result);
                            }
                        } else {
                            ++run->failures;
                            qWarning().noquote() << "Provider search failed for"
                                << providerName << searchProcess->readAllStandardError();
                        }
                        searchProcess->deleteLater();
                        --run->active;
                        --run->remaining;
                        if (artworkContext) {
                            appendSearchResults(results, typeFilter, batch,
                                                &run->seen, artworkContext,
                                                searchResultProviderFilter);
                        }
                        if (run->remaining == 0) {
                            if (results->count() == 0) {
                                results->addItem(run->failures > 0
                                    ? "Search failed in " + QString::number(run->failures) + " provider(s)."
                                    : "No results found");
                            }
                            status->setText(QString::number(run->seen.size()) +
                                " CloudStream result(s)" +
                                (run->failures > 0
                                    ? " • " + QString::number(run->failures) + " provider(s) failed"
                                    : QString()));
                            run->launchMore = {};
                            return;
                        }
                        status->setText(QString::number(run->seen.size()) +
                            " result(s) • searching " +
                            QString::number(run->remaining) + " provider(s)");
                        if (run->launchMore) run->launchMore();
                    };
                    connect(searchProcess,
                            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                            this, [finishProvider](int exitCode, QProcess::ExitStatus) {
                        finishProvider(exitCode, false);
                    });
                    connect(searchProcess, &QProcess::errorOccurred, this,
                            [finishProvider](QProcess::ProcessError error) {
                        if (error == QProcess::FailedToStart) finishProvider(-1, true);
                    });
                    searchProcess->start();
                    QTimer::singleShot(20000, searchProcess, [searchProcess] {
                        if (searchProcess->state() != QProcess::NotRunning) searchProcess->kill();
                    });
                }
            };
            run->launchMore();
        };
        searchAutomation = [query, search](const QString &term) {
            query->setText(term);
            (*search)();
        };
        connect(submitAction, &QAction::triggered, this, [search] { (*search)(); });
        connect(query, &QLineEdit::returnPressed, this, [search] { (*search)(); });
        connect(providerAction, &QAction::triggered, this, [this, query, search] {
            if (showSearchProviderDialog() && !query->text().trimmed().isEmpty()) (*search)();
        });
        connect(searchProviderButton, &QPushButton::clicked, this, [this, query, search] {
            if (showSearchProviderDialog() && !query->text().trimmed().isEmpty()) (*search)();
        });
        connect(historyList, &QListWidget::itemActivated, this, [query, search](QListWidgetItem *item) {
            query->setText(item->data(Qt::UserRole).toString());
            (*search)();
        });
        connect(clearHistory, &QPushButton::clicked, this, [this, refreshHistory] {
            settings.remove("searchHistory");
            refreshHistory();
        });
        for (auto *chip : typeFilter->buttons()) {
            connect(chip, &QAbstractButton::toggled, this, [this, results, typeFilter](bool checked) {
                if (checked && results->count() > 0) applySearchTypeFilter(results, typeFilter);
            });
        }
        connect(searchResultProviderFilter, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this, results, typeFilter] {
            applySearchFilters(results, typeFilter, searchResultProviderFilter);
        });
        refreshSearchProviderButton();
        connect(results, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            const auto url = item->data(Qt::UserRole).toString();
            const auto provider = item->data(Qt::UserRole + 1).toString();
            const auto jar = item->data(Qt::UserRole + 2).toString();
            if (!url.isEmpty() && !provider.isEmpty() && !jar.isEmpty()) openDetails(jar, provider, url);
        });
        return page;
    }

    void showInAppFolderDialog(const QString &initialPath,
                               const QString &titleText,
                               bool selectFolder,
                               const std::function<void(const QString &)> &accepted) {
        const bool selectFile = !selectFolder && static_cast<bool>(accepted);
        auto *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setObjectName("cloudStreamFolderDialog");
        dialog->setWindowTitle(titleText);
        dialog->resize(720, 520);
        dialog->setMinimumSize(560, 400);
        dialog->setModal(true);
        dialog->setStyleSheet(
            "QDialog{background:#0b0b0e;color:#f5f4f8;}"
            "QLineEdit{background:#181b24;color:#f5f4f8;border:1px solid #292d39;border-radius:16px;padding:9px 13px;}"
            "QListWidget{background:#101218;color:#eeeeF2;border:1px solid #292d39;border-radius:14px;padding:6px;}"
            "QListWidget::item{padding:10px;border-radius:9px;}"
            "QListWidget::item:hover{background:#1c202a;}"
            "QListWidget::item:selected{background:#1b2754;color:white;}"
            "QPushButton{min-height:38px;padding:0 16px;background:#222632;color:#f5f4f8;border:0;border-radius:10px;}"
            "QPushButton:hover{background:#30384a;}"
            "QPushButton[primary=\"true\"]{background:#536dfe;color:white;}"
        );
        auto *root = new QVBoxLayout(dialog);
        root->setContentsMargins(20, 18, 20, 18);
        root->setSpacing(10);
        auto *path = new QLineEdit(QDir(initialPath).absolutePath(), dialog);
        path->setReadOnly(true);
        root->addWidget(path);
        auto *list = new QListWidget(dialog);
        root->addWidget(list, 1);
        auto *buttons = new QHBoxLayout;
        auto *up = button("Up");
        auto *cancel = button("Cancel");
        auto *accept = button(selectFolder ? "Select folder" : selectFile ? "Open file" : "Close", true);
        buttons->addWidget(up);
        buttons->addStretch();
        buttons->addWidget(cancel);
        buttons->addWidget(accept);
        root->addLayout(buttons);
        auto currentPath = std::make_shared<QString>(QDir(initialPath).absolutePath());
        const auto populate = [list, path, currentPath, selectFolder] {
            QDir directory(*currentPath);
            path->setText(directory.absolutePath());
            list->clear();
            const auto folders = directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                          QDir::Name | QDir::IgnoreCase);
            for (const auto &folder : folders) {
                auto *item = new QListWidgetItem(QIcon(":/icons/library-outline.svg"), folder.fileName());
                item->setData(Qt::UserRole, folder.absoluteFilePath());
                list->addItem(item);
            }
            if (!selectFolder) {
                const auto files = directory.entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase);
                for (const auto &file : files) {
                    auto *item = new QListWidgetItem(file.fileName());
                    item->setData(Qt::UserRole, file.absoluteFilePath());
                    item->setToolTip(file.absoluteFilePath());
                    list->addItem(item);
                }
            }
        };
        connect(list, &QListWidget::itemActivated, dialog, [dialog, currentPath, populate, selectFile, accepted](QListWidgetItem *item) {
            const auto selected = item->data(Qt::UserRole).toString();
            if (QFileInfo(selected).isDir()) {
                *currentPath = QDir(selected).absolutePath();
                populate();
            } else if (selectFile) {
                accepted(selected);
                dialog->accept();
            }
        });
        connect(up, &QPushButton::clicked, dialog, [currentPath, populate] {
            const auto parent = QDir(*currentPath).absolutePath();
            if (QDir(parent).cdUp()) {
                *currentPath = QDir(parent).absolutePath();
                populate();
            }
        });
        connect(cancel, &QPushButton::clicked, dialog, &QDialog::reject);
        connect(accept, &QPushButton::clicked, dialog, [dialog, list, currentPath, selectFolder, selectFile, accepted] {
            if (selectFolder) accepted(*currentPath);
            else if (selectFile && list->currentItem() &&
                     QFileInfo(list->currentItem()->data(Qt::UserRole).toString()).isFile())
                accepted(list->currentItem()->data(Qt::UserRole).toString());
            dialog->accept();
        });
        populate();
        dialog->open();
    }

    QWidget *libraryPage() {
        auto *page = pageFrame("Library", {});
        auto *v = qobject_cast<QVBoxLayout *>(page->layout());
        auto *tabs = new QTabBar;
        tabs->setObjectName("libraryTabs");
        tabs->setExpanding(false);
        tabs->setDrawBase(false);
        tabs->addTab("All");
        tabs->addTab("Watching");
        tabs->addTab("Completed");
        tabs->addTab("Paused");
        tabs->addTab("Cancelled");
        v->addWidget(tabs);

        auto *tools = new QHBoxLayout;
        auto *filter = new QLineEdit;
        filter->setPlaceholderText("Search your library…");
        filter->addAction(QIcon(":/icons/search.svg"), QLineEdit::LeadingPosition);
        filter->setMinimumHeight(42);
        auto *sort = new QComboBox;
        sort->addItems({"Recently watched", "Name", "Oldest first"});
        sort->setMinimumHeight(42);
        sort->setMinimumWidth(180);
        auto *choose = button("Media folder");
        choose->setIcon(QIcon(":/icons/library-outline.svg"));
        choose->setMinimumWidth(154);
        tools->addWidget(filter, 1);
        tools->addWidget(sort);
        tools->addWidget(choose);
        v->addLayout(tools);
        auto *summary = new QLabel;
        summary->setObjectName("muted");
        v->addWidget(summary);

        auto *list = new QListWidget;
        list->setObjectName("mediaList");
        list->setViewMode(QListView::IconMode);
        list->setResizeMode(QListView::Adjust);
        list->setMovement(QListView::Static);
        list->setUniformItemSizes(true);
        list->setLayoutMode(QListView::Batched);
        list->setBatchSize(32);
        list->setIconSize(CloudStream::ArtworkSizing::posterSize(150));
        list->setGridSize(QSize(180, 290));
        list->setSpacing(8);
        list->setWordWrap(true);
        auto *empty = new QWidget;
        auto *emptyLayout = new QVBoxLayout(empty);
        emptyLayout->addStretch();
        auto *emptyIcon = new QLabel;
        emptyIcon->setPixmap(QIcon(":/icons/library-outline.svg").pixmap(QSize(52, 52)));
        emptyIcon->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyIcon);
        auto *emptyTitle = title("Your library is empty", 20);
        emptyTitle->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyTitle);
        auto *emptyMessage = new QLabel("Titles you watch or mark will appear here");
        emptyMessage->setObjectName("muted");
        emptyMessage->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyMessage);
        emptyLayout->addStretch();
        auto *content = new QStackedWidget;
        content->addWidget(empty);
        content->addWidget(list);
        auto localFileCache = std::make_shared<QList<QFileInfo>>();
        auto localFileCacheRoots = std::make_shared<QString>();
        auto localFileCacheValid = std::make_shared<bool>(false);
        auto refresh = [list, content, emptyMessage, summary, this, tabs, filter, sort,
                        localFileCache, localFileCacheRoots, localFileCacheValid] {
            list->setUpdatesEnabled(false);
            list->clear();
            static const QStringList states{"Watching", "Completed", "Paused", "Cancelled"};
            const auto state = tabs->currentIndex() == 0 ? QString() : states.value(tabs->currentIndex() - 1);
            auto providerEntries = history.entries(state);
            const auto searchTerm = filter->text().trimmed();
            providerEntries.erase(std::remove_if(providerEntries.begin(), providerEntries.end(), [&searchTerm](const CloudStream::WatchEntry &entry) {
                return !searchTerm.isEmpty() && !entry.name.contains(searchTerm, Qt::CaseInsensitive) &&
                    !entry.episodeName.contains(searchTerm, Qt::CaseInsensitive);
            }), providerEntries.end());
            if (sort->currentIndex() == 1) {
                std::sort(providerEntries.begin(), providerEntries.end(), [](const auto &left, const auto &right) {
                    return left.name.localeAwareCompare(right.name) < 0;
                });
            } else if (sort->currentIndex() == 2) {
                std::reverse(providerEntries.begin(), providerEntries.end());
            }
            for (const auto &entry : providerEntries) {
                QStringList facts{entry.name};
                if (!entry.episodeName.isEmpty()) facts << entry.episodeName;
                if (entry.durationSeconds > 0.0) {
                    const auto percent = qBound(0, qRound((entry.positionSeconds / entry.durationSeconds) * 100.0), 100);
                    facts << QString::number(percent) + "%";
                }
                auto *item = new QListWidgetItem(QIcon(":/assets/cloudstream-launcher.png"), facts.join("\n"));
                item->setData(Qt::UserRole, entry.sourceUrl);
                item->setData(Qt::UserRole + 1, "provider");
                item->setData(Qt::UserRole + 2, entry.id);
                item->setData(Qt::UserRole + 3, entry.provider);
                item->setData(Qt::UserRole + 4, entry.jarPath);
                item->setData(Qt::UserRole + 5, entry.playbackData);
                item->setToolTip(entry.sourceUrl);
                list->addItem(item);
                if (!entry.posterUrl.isEmpty()) {
                    QPointer<QListWidget> safeList = list;
                    const QPersistentModelIndex itemIndex(
                        list->model()->index(list->row(item), 0));
                    const auto priority = list->count() <= 18
                        ? CloudStream::ArtworkLoader::HighPriority
                        : CloudStream::ArtworkLoader::NormalPriority;
                    artworkLoader->load(QUrl(entry.posterUrl),
                        CloudStream::ArtworkSizing::posterSize(150), list,
                        [safeList, itemIndex](const QImage &image) {
                            if (!safeList || !itemIndex.isValid()) return;
                            if (auto *liveItem = safeList->item(itemIndex.row())) {
                                liveItem->setIcon(QIcon(QPixmap::fromImage(image)));
                            }
                        }, priority);
                }
            }
            if (tabs->currentIndex() == 0) {
                QStringList roots;
                roots << settings.value("libraryFolder").toString() << QDir::homePath() + "/Videos";
                roots.removeDuplicates();
                const auto rootKey = roots.join('\n');
                if (!*localFileCacheValid || *localFileCacheRoots != rootKey) {
                    localFileCache->clear();
                    for (const auto &root : roots) {
                        if (root.isEmpty() || !QDir(root).exists()) continue;
                        QDirIterator it(root, {"*.mp4", "*.mkv", "*.webm", "*.avi", "*.mov", "*.m4v", "*.mp3", "*.flac", "*.ogg"}, QDir::Files, QDirIterator::Subdirectories);
                        while (it.hasNext() && localFileCache->size() < 500) {
                            localFileCache->append(QFileInfo(it.next()));
                        }
                    }
                    *localFileCacheRoots = rootKey;
                    *localFileCacheValid = true;
                }
                auto files = *localFileCache;
                files.erase(std::remove_if(files.begin(), files.end(), [&searchTerm](const QFileInfo &file) {
                    return !searchTerm.isEmpty() &&
                           !file.fileName().contains(searchTerm, Qt::CaseInsensitive);
                }), files.end());
                std::sort(files.begin(), files.end(), [sort](const QFileInfo &a, const QFileInfo &b) {
                    if (sort->currentIndex() == 0) return a.lastModified() > b.lastModified();
                    if (sort->currentIndex() == 2) return a.lastModified() < b.lastModified();
                    return a.fileName().localeAwareCompare(b.fileName()) < 0;
                });
                for (const auto &file : files) {
                    auto *item = new QListWidgetItem(QIcon(":/icons/library-outline.svg"), file.completeBaseName() + "\nLocal file");
                    item->setData(Qt::UserRole, file.absoluteFilePath());
                    item->setData(Qt::UserRole + 1, "local");
                    item->setToolTip(file.absoluteFilePath());
                    list->addItem(item);
                }
            }
            list->setUpdatesEnabled(true);
            if (list->count() == 0) {
                summary->hide();
                emptyMessage->setText(state.isEmpty() ? "Titles you watch or mark will appear here"
                                                       : "No titles are marked " + state.toLower());
                content->setCurrentWidget(content->widget(0));
            } else {
                summary->setText(QString::number(list->count()) + (list->count() == 1 ? " title" : " titles"));
                summary->show();
                content->setCurrentWidget(list);
            }
        };
        libraryRefresh = refresh;
        v->addWidget(content, 1);
        auto *selectionBar = new QWidget;
        selectionBar->setObjectName("selectionBar");
        auto *actions = new QHBoxLayout(selectionBar);
        actions->setContentsMargins(12, 8, 12, 8);
        auto *play = button("Play", true);
        play->setIcon(QIcon(":/icons/play-dark.svg"));
        auto *playWithSubtitles = button("Subtitles…");
        auto *details = button("Details");
        details->setIcon(QIcon(":/icons/info.svg"));
        auto *stateTarget = new QComboBox;
        stateTarget->addItems({"Watching", "Completed", "Paused", "Cancelled"});
        stateTarget->setMinimumHeight(42);
        auto *move = button("Move");
        auto *removeFromLibrary = button("Remove from library");
        removeFromLibrary->setProperty("danger", true);
        removeFromLibrary->setIcon(QIcon(":/icons/delete.svg"));
        actions->addWidget(play);
        actions->addWidget(details);
        actions->addWidget(playWithSubtitles);
        actions->addStretch();
        actions->addWidget(stateTarget);
        actions->addWidget(move);
        actions->addWidget(removeFromLibrary);
        selectionBar->hide();
        v->addWidget(selectionBar);
        connect(choose, &QPushButton::clicked, this,
                [this, refresh, localFileCacheValid] {
            showInAppFolderDialog(settings.value("libraryFolder", QDir::homePath()).toString(),
                                  "Choose media folder", true, [this, refresh, localFileCacheValid](const QString &folder) {
                settings.setValue("libraryFolder", folder);
                *localFileCacheValid = false;
                refresh();
            });
        });
        auto selectedItem = [list]() -> QListWidgetItem * {
            if (list->currentItem()) return list->currentItem();
            const auto selected = list->selectedItems();
            return selected.isEmpty() ? nullptr : selected.first();
        };
        auto playSelected = [selectedItem, this](const QString &subtitle = {}) {
            auto *item = selectedItem();
            if (!item || !item->data(Qt::UserRole).isValid()) return;
            if (item->data(Qt::UserRole + 1).toString() == "provider") {
                const auto data = item->data(Qt::UserRole + 5).toString();
                if (!data.isEmpty()) {
                    resolveAndPlay(item->data(Qt::UserRole + 4).toString(), item->data(Qt::UserRole + 3).toString(),
                                   data, item->data(Qt::UserRole + 2).toString(), item->text());
                } else {
                    openDetails(item->data(Qt::UserRole + 4).toString(), item->data(Qt::UserRole + 3).toString(),
                                item->data(Qt::UserRole).toString());
                }
                return;
            }
            startPlayer(item->data(Qt::UserRole).toString(), subtitle);
        };
        connect(play, &QPushButton::clicked, this, [playSelected] { playSelected(); });
        connect(playWithSubtitles, &QPushButton::clicked, this, [selectedItem, this] {
            auto *item = selectedItem();
            if (!item || !item->data(Qt::UserRole).isValid()) return;
            if (item->data(Qt::UserRole + 1).toString() != "local") {
                status->setText("Local subtitle selection currently applies to local files");
                return;
            }
            showInAppFolderDialog(QDir::homePath(), "Choose subtitle", false,
                                  [this, item](const QString &subtitle) {
                startPlayer(item->data(Qt::UserRole).toString(), subtitle);
            });
        });
        connect(details, &QPushButton::clicked, this, [selectedItem, this] {
            auto *item = selectedItem();
            if (!item || item->data(Qt::UserRole + 1).toString() != "provider") return;
            openDetails(item->data(Qt::UserRole + 4).toString(), item->data(Qt::UserRole + 3).toString(),
                        item->data(Qt::UserRole).toString());
        });
        connect(move, &QPushButton::clicked, this, [this, selectedItem, stateTarget, refresh] {
            auto *item = selectedItem();
            const auto id = item ? item->data(Qt::UserRole + 2).toString() : QString();
            if (id.isEmpty()) return;
            if (history.setState(id, stateTarget->currentText())) {
                refresh();
                refreshContinueWatching();
                status->setText("Moved title to " + stateTarget->currentText());
            }
        });
        connect(removeFromLibrary, &QPushButton::clicked, this,
                [this, selectedItem, refresh] {
            auto *item = selectedItem();
            if (!item) return;
            if (item->data(Qt::UserRole + 1).toString() != "provider") {
                status->setText("Local media stays on disk; remove its folder to hide it");
                return;
            }
            const auto id = item->data(Qt::UserRole + 2).toString();
            if (history.remove(id)) {
                refresh();
                refreshContinueWatching();
                status->setText("Removed from library");
            }
        });
        connect(list, &QListWidget::itemActivated, this, [list, playSelected](QListWidgetItem *item) {
            list->setCurrentItem(item);
            playSelected();
        });
        connect(list, &QListWidget::currentItemChanged, this,
                [selectionBar, removeFromLibrary](QListWidgetItem *current) {
            selectionBar->setVisible(current && current->data(Qt::UserRole).isValid());
            removeFromLibrary->setEnabled(current &&
                current->data(Qt::UserRole + 1).toString() == "provider");
        });
        connect(tabs, &QTabBar::currentChanged, this, [refresh] { refresh(); });
        auto *filterDebounce = new QTimer(page);
        filterDebounce->setSingleShot(true);
        filterDebounce->setInterval(140);
        connect(filterDebounce, &QTimer::timeout, this, [refresh] { refresh(); });
        connect(filter, &QLineEdit::textChanged, filterDebounce,
                qOverload<>(&QTimer::start));
        connect(sort, qOverload<int>(&QComboBox::currentIndexChanged), this, [refresh] { refresh(); });
        return page;
    }

    QWidget *downloadsPage() {
        auto *page = pageFrame("Downloads", {});
        auto *v = qobject_cast<QVBoxLayout *>(page->layout());
        const auto downloadPath = settings.value("downloadFolder", QDir::homePath() + "/Downloads").toString();
        QDir().mkpath(downloadPath);

        auto *toolbar = new QHBoxLayout;
        auto *stream = button("Network stream");
        stream->setIcon(QIcon(":/icons/network-stream.svg"));
        auto *open = button("Open folder");
        open->setIcon(QIcon(":/icons/library-outline.svg"));
        auto *refresh = button("Refresh");
        refresh->setIcon(QIcon(":/icons/refresh.svg"));
        toolbar->addWidget(stream);
        toolbar->addWidget(open);
        toolbar->addStretch();
        toolbar->addWidget(refresh);
        v->addLayout(toolbar);

        auto *storagePanel = new QWidget;
        storagePanel->setObjectName("storagePanel");
        auto *storageLayout = new QHBoxLayout(storagePanel);
        storageLayout->setContentsMargins(18, 14, 18, 14);
        auto *storageHeading = title("Storage", 17);
        storageLayout->addWidget(storageHeading);
        auto *usage = new QProgressBar;
        usage->setTextVisible(false);
        usage->setRange(0, 1000);
        usage->setMinimumWidth(260);
        storageLayout->addWidget(usage, 1);
        auto *storageText = new QLabel;
        storageText->setObjectName("muted");
        storageLayout->addWidget(storageText);
        v->addWidget(storagePanel);

        auto *tabs = new QTabBar;
        tabs->setObjectName("downloadTabs");
        tabs->setDrawBase(false);
        tabs->setExpanding(false);
        tabs->addTab("All");
        tabs->addTab("Active");
        tabs->addTab("Completed");
        tabs->addTab("Needs attention");
        v->addWidget(tabs);

        auto *selectionActions = new QHBoxLayout;
        auto *pause = button("Pause");
        auto *resume = button("Resume");
        auto *play = button("Play");
        play->setIcon(QIcon(":/icons/play.svg"));
        auto *remove = button("Remove");
        remove->setProperty("danger", true);
        remove->setIcon(QIcon(":/icons/delete.svg"));
        selectionActions->addWidget(pause);
        selectionActions->addWidget(resume);
        selectionActions->addWidget(play);
        selectionActions->addStretch();
        selectionActions->addWidget(remove);
        v->addLayout(selectionActions);

        auto *downloads = new QListWidget;
        downloads->setObjectName("downloadList");
        downloads->setIconSize(QSize(42, 42));
        downloads->setSpacing(4);
        auto *empty = new QWidget;
        auto *emptyLayout = new QVBoxLayout(empty);
        emptyLayout->addStretch();
        auto *emptyIcon = new QLabel;
        emptyIcon->setPixmap(QIcon(":/icons/download.svg").pixmap(QSize(52, 52)));
        emptyIcon->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyIcon);
        auto *emptyTitle = title("No downloads", 20);
        emptyTitle->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyTitle);
        auto *emptyMessage = new QLabel("Downloaded episodes and movies will appear here");
        emptyMessage->setObjectName("muted");
        emptyMessage->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyMessage);
        emptyLayout->addStretch();
        auto *content = new QStackedWidget;
        content->addWidget(empty);
        content->addWidget(downloads);
        v->addWidget(content, 1);

        const auto stateLabel = [](CloudStream::DownloadState state) {
            switch (state) {
            case CloudStream::DownloadState::Queued: return QStringLiteral("Queued");
            case CloudStream::DownloadState::Downloading: return QStringLiteral("Downloading");
            case CloudStream::DownloadState::Paused: return QStringLiteral("Paused");
            case CloudStream::DownloadState::Completed: return QStringLiteral("Completed");
            case CloudStream::DownloadState::Failed: return QStringLiteral("Failed");
            }
            return QStringLiteral("Failed");
        };
        const auto progressText = [stateLabel](const CloudStream::DownloadEntry &entry) {
            QStringList facts{stateLabel(entry.state)};
            if (entry.bytesTotal > 0) {
                const auto percent = qBound(0, int((entry.bytesReceived * 100) / entry.bytesTotal), 100);
                facts << QString::number(percent) + "%";
                facts << QLocale().formattedDataSize(entry.bytesReceived) + " of " +
                         QLocale().formattedDataSize(entry.bytesTotal);
            } else if (entry.bytesReceived > 0) {
                facts << QLocale().formattedDataSize(entry.bytesReceived);
            }
            if (!entry.error.isEmpty()) facts << entry.error;
            return facts.join("  •  ");
        };
        const auto addRow = [downloads](QListWidgetItem *item, const QString &iconPath,
                                        const QString &heading, const QString &summary,
                                        qint64 received, qint64 total, bool showProgress) {
            item->setText({});
            item->setSizeHint(QSize(0, 74));
            downloads->addItem(item);
            auto *row = new QWidget;
            row->setObjectName("downloadRow");
            row->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto *layout = new QHBoxLayout(row);
            layout->setContentsMargins(10, 7, 12, 7);
            layout->setSpacing(12);
            auto *icon = new QLabel;
            icon->setPixmap(QIcon(iconPath).pixmap(QSize(30, 30)));
            icon->setFixedSize(34, 34);
            icon->setAlignment(Qt::AlignCenter);
            layout->addWidget(icon);
            auto *labels = new QVBoxLayout;
            labels->setSpacing(3);
            auto *titleLabel = new QLabel(heading);
            titleLabel->setObjectName("downloadRowTitle");
            titleLabel->setTextFormat(Qt::PlainText);
            auto *summaryLabel = new QLabel(summary);
            summaryLabel->setObjectName("downloadRowSummary");
            summaryLabel->setTextFormat(Qt::PlainText);
            labels->addWidget(titleLabel);
            labels->addWidget(summaryLabel);
            layout->addLayout(labels, 1);
            auto *progress = new QProgressBar;
            progress->setObjectName("downloadRowProgress");
            progress->setTextVisible(false);
            progress->setFixedWidth(190);
            progress->setRange(0, 1000);
            progress->setValue(total > 0 ? int((std::min(received, total) * 1000) / total) : 0);
            progress->setVisible(showProgress && total > 0);
            layout->addWidget(progress);
            downloads->setItemWidget(item, row);
        };
        auto updateActions = [downloads, pause, resume, play, remove] {
            auto *item = downloads->currentItem();
            const auto id = item ? item->data(Qt::UserRole + 1).toString() : QString();
            const auto stateValue = item ? item->data(Qt::UserRole + 2).toInt() : -1;
            const auto state = static_cast<CloudStream::DownloadState>(stateValue);
            const auto path = item ? item->data(Qt::UserRole).toString() : QString();
            const bool queued = !id.isEmpty();
            const bool missingCompleted = queued && state == CloudStream::DownloadState::Completed &&
                                          !QFileInfo::exists(path);
            const bool canResume = queued && (state == CloudStream::DownloadState::Paused ||
                                              state == CloudStream::DownloadState::Failed || missingCompleted);
            const bool canPlay = item && state == CloudStream::DownloadState::Completed &&
                                 QFileInfo::exists(path);
            pause->setEnabled(queued && (state == CloudStream::DownloadState::Queued ||
                                         state == CloudStream::DownloadState::Downloading));
            resume->setEnabled(canResume);
            resume->setText(missingCompleted ? "Download again"
                : state == CloudStream::DownloadState::Failed ? "Retry" : "Resume");
            resume->setToolTip(missingCompleted ? "Download the missing file again using its saved source"
                : state == CloudStream::DownloadState::Failed ? "Retry from the saved partial file"
                                                               : "Continue this download");
            play->setEnabled(canPlay);
            play->setToolTip(play->isEnabled() ? "Play the completed local file"
                                               : "Playback is available after the download completes");
            const auto setPrimary = [](QPushButton *action, bool primary) {
                if (action->property("primary").toBool() == primary) return;
                action->setProperty("primary", primary);
                action->style()->unpolish(action);
                action->style()->polish(action);
            };
            setPrimary(resume, canResume);
            setPrimary(play, canPlay);
            play->setIcon(QIcon(canPlay ? ":/icons/play-dark.svg" : ":/icons/play.svg"));
            remove->setEnabled(item != nullptr && !path.isEmpty());
        };
        auto reload = [this, downloads, content, emptyMessage, tabs, usage, storageText,
                       downloadPath, progressText, addRow, updateActions] {
            const auto selectedId = downloads->currentItem()
                ? downloads->currentItem()->data(Qt::UserRole + 1).toString() : QString();
            const auto selectedPath = downloads->currentItem()
                ? downloads->currentItem()->data(Qt::UserRole).toString() : QString();
            downloads->clear();
            QStorageInfo storage(downloadPath);
            const auto used = storage.bytesTotal() - storage.bytesAvailable();
            usage->setValue(storage.bytesTotal() > 0 ? int((used * 1000) / storage.bytesTotal()) : 0);
            storageText->setText(QLocale().formattedDataSize(used) + " used  •  " +
                                 QLocale().formattedDataSize(storage.bytesAvailable()) + " free");
            const auto queueEntries = downloadQueue.entries();
            QSet<QString> queuedPaths;
            int activeCount = 0;
            int completedCount = 0;
            int attentionCount = 0;
            for (const auto &entry : queueEntries) {
                queuedPaths.insert(QFileInfo(entry.targetPath).absoluteFilePath());
                queuedPaths.insert(QFileInfo(entry.partPath()).absoluteFilePath());
                const bool missing = entry.state == CloudStream::DownloadState::Completed &&
                                     !QFileInfo::exists(entry.targetPath);
                const bool active = entry.state == CloudStream::DownloadState::Queued ||
                                    entry.state == CloudStream::DownloadState::Downloading ||
                                    entry.state == CloudStream::DownloadState::Paused;
                const bool attention = entry.state == CloudStream::DownloadState::Failed || missing;
                if (active) ++activeCount;
                if (entry.state == CloudStream::DownloadState::Completed && !missing) ++completedCount;
                if (attention) ++attentionCount;
            }
            tabs->setTabText(0, "All (" + QString::number(queueEntries.size()) + ")");
            tabs->setTabText(1, "Active (" + QString::number(activeCount) + ")");
            tabs->setTabText(2, "Completed (" + QString::number(completedCount) + ")");
            tabs->setTabText(3, "Needs attention (" + QString::number(attentionCount) + ")");
            for (const auto &entry : queueEntries) {
                const bool missing = entry.state == CloudStream::DownloadState::Completed &&
                                     !QFileInfo::exists(entry.targetPath);
                const bool active = entry.state == CloudStream::DownloadState::Queued ||
                                    entry.state == CloudStream::DownloadState::Downloading ||
                                    entry.state == CloudStream::DownloadState::Paused;
                const bool attention = entry.state == CloudStream::DownloadState::Failed || missing;
                if (tabs->currentIndex() == 1 && !active) continue;
                if (tabs->currentIndex() == 2 && (entry.state != CloudStream::DownloadState::Completed || missing)) continue;
                if (tabs->currentIndex() == 3 && !attention) continue;
                auto summary = progressText(entry);
                if (missing) {
                    summary = "Missing file  •  Select this item to Download again or Remove";
                }
                QString iconPath = ":/icons/download-active.svg";
                if (entry.state == CloudStream::DownloadState::Paused) iconPath = ":/icons/pause.svg";
                else if (entry.state == CloudStream::DownloadState::Failed) iconPath = ":/icons/download-error.svg";
                else if (missing) iconPath = ":/icons/warning.svg";
                else if (entry.state == CloudStream::DownloadState::Completed) iconPath = ":/icons/check.svg";
                auto *item = new QListWidgetItem;
                item->setData(Qt::UserRole, entry.targetPath);
                item->setData(Qt::UserRole + 1, entry.id);
                item->setData(Qt::UserRole + 2, int(entry.state));
                item->setData(Qt::UserRole + 3, entry.title);
                item->setToolTip(entry.targetPath + (entry.error.isEmpty() ? QString() : "\n" + entry.error));
                item->setData(Qt::AccessibleTextRole, entry.title + ", " + summary);
                addRow(item, iconPath, entry.title, summary, entry.bytesReceived,
                       entry.bytesTotal, active || entry.state == CloudStream::DownloadState::Failed);
            }
            const auto files = QDir(downloadPath).entryInfoList(
                {"*.mp4", "*.mkv", "*.webm", "*.avi", "*.mov", "*.m4v", "*.ts", "*.part"},
                QDir::Files, QDir::Time);
            for (const auto &file : files) {
                if (queuedPaths.contains(file.absoluteFilePath())) continue;
                const bool incomplete = file.suffix().compare("part", Qt::CaseInsensitive) == 0;
                if (tabs->currentIndex() == 1 && !incomplete) continue;
                if (tabs->currentIndex() == 2 && incomplete) continue;
                if (tabs->currentIndex() == 3 && !incomplete) continue;
                const QString state = incomplete ? "Incomplete • source metadata unavailable" : "Completed";
                const auto summary = state + "  •  " + QLocale().formattedDataSize(file.size());
                auto *item = new QListWidgetItem;
                item->setData(Qt::UserRole, file.absoluteFilePath());
                item->setData(Qt::UserRole + 1, QString());
                item->setData(Qt::UserRole + 2, incomplete ? -1 : int(CloudStream::DownloadState::Completed));
                item->setData(Qt::UserRole + 3, file.completeBaseName());
                item->setToolTip(file.absoluteFilePath());
                item->setData(Qt::AccessibleTextRole, file.completeBaseName() + ", " + summary);
                addRow(item, incomplete ? ":/icons/warning.svg" : ":/icons/check.svg",
                       file.completeBaseName(), summary, 0, 0, false);
            }
            if (downloads->count() == 0) {
                emptyMessage->setText(tabs->currentIndex() == 1 ? "No queued, downloading, or paused items"
                                      : tabs->currentIndex() == 2 ? "No completed downloads yet"
                                      : tabs->currentIndex() == 3 ? "No downloads need attention"
                                                                  : "Downloaded episodes and movies will appear here");
                content->setCurrentWidget(content->widget(0));
            } else {
                content->setCurrentWidget(downloads);
                int restoreRow = -1;
                for (int row = 0; row < downloads->count(); ++row) {
                    auto *item = downloads->item(row);
                    if ((!selectedId.isEmpty() && item->data(Qt::UserRole + 1).toString() == selectedId) ||
                        (selectedId.isEmpty() && !selectedPath.isEmpty() &&
                         item->data(Qt::UserRole).toString() == selectedPath)) {
                        restoreRow = row;
                        break;
                    }
                }
                downloads->setCurrentRow(restoreRow >= 0 ? restoreRow : 0);
            }
            updateActions();
        };
        downloadsRefresh = reload;
        reload();
        connect(refresh, &QPushButton::clicked, this, reload);
        connect(tabs, &QTabBar::currentChanged, this, [reload] { reload(); });
        connect(downloads, &QListWidget::currentItemChanged, this,
                [updateActions](QListWidgetItem *, QListWidgetItem *) { updateActions(); });
        connect(downloads, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            const auto path = item->data(Qt::UserRole).toString();
            const auto state = static_cast<CloudStream::DownloadState>(item->data(Qt::UserRole + 2).toInt());
            if (!path.isEmpty() && state == CloudStream::DownloadState::Completed && QFileInfo::exists(path)) {
                startPlayer(path);
            }
        });
        connect(pause, &QPushButton::clicked, this, [this, downloads] {
            if (auto *item = downloads->currentItem()) downloadManager.pause(item->data(Qt::UserRole + 1).toString());
        });
        connect(resume, &QPushButton::clicked, this, [this, downloads] {
            if (auto *item = downloads->currentItem()) downloadManager.resume(item->data(Qt::UserRole + 1).toString());
        });
        connect(play, &QPushButton::clicked, this, [this, downloads] {
            if (auto *item = downloads->currentItem()) {
                const auto path = item->data(Qt::UserRole).toString();
                if (QFileInfo::exists(path)) startPlayer(path);
            }
        });
        connect(remove, &QPushButton::clicked, this, [this, downloads, reload] {
            auto *item = downloads->currentItem();
            if (!item) return;
            const auto id = item->data(Qt::UserRole + 1).toString();
            const auto path = item->data(Qt::UserRole).toString();
            const auto title = item->data(Qt::UserRole + 3).toString();
            QMessageBox confirmation(QMessageBox::Warning, "Remove download",
                "Remove “" + title + "” and delete its downloaded data?",
                QMessageBox::NoButton, this);
            auto *deleteButton = confirmation.addButton("Remove and delete", QMessageBox::DestructiveRole);
            confirmation.addButton(QMessageBox::Cancel);
            confirmation.exec();
            if (confirmation.clickedButton() != deleteButton) return;
            if (!id.isEmpty()) downloadManager.remove(id, true);
            else if (!path.isEmpty()) { QFile::remove(path); reload(); }
        });
        connect(&downloadManager, &CloudStream::DownloadManager::progressChanged, page,
                [downloads](const QString &id, qint64 received, qint64 total) {
            for (int row = 0; row < downloads->count(); ++row) {
                auto *item = downloads->item(row);
                if (item->data(Qt::UserRole + 1).toString() != id) continue;
                QStringList facts{"Downloading"};
                if (total > 0) facts << QString::number(qBound(0, int((received * 100) / total), 100)) + "%";
                facts << QLocale().formattedDataSize(received) +
                         (total > 0 ? " of " + QLocale().formattedDataSize(total) : QString());
                item->setData(Qt::AccessibleTextRole,
                              item->data(Qt::UserRole + 3).toString() + ", " + facts.join("  •  "));
                if (auto *rowWidget = downloads->itemWidget(item)) {
                    if (auto *summary = rowWidget->findChild<QLabel *>("downloadRowSummary")) {
                        summary->setText(facts.join("  •  "));
                    }
                    if (auto *progress = rowWidget->findChild<QProgressBar *>("downloadRowProgress")) {
                        progress->setVisible(total > 0);
                        if (total > 0) progress->setValue(
                            int((std::min(received, total) * 1000) / total));
                    }
                }
                break;
            }
        });
        connect(stream, &QPushButton::clicked, this, [this] { showNetworkStreamDialog(); });
        connect(open, &QPushButton::clicked, this, [this, downloadPath] {
            showInAppFolderDialog(downloadPath, "Downloads", false, {});
        });
        return page;
    }

    QString currentRepositoryUrl() const {
        if (!repositories || !repositories->currentItem()) return {};
        return repositories->currentItem()->data(Qt::UserRole).toString();
    }

    void loadExtensionIcon(QListWidget *list, QListWidgetItem *item, QString iconUrl) {
        if (!list || !item || iconUrl.isEmpty()) return;
        iconUrl.replace("%exact_size%", "64");
        iconUrl.replace("%size%", "64");
        const QUrl url(iconUrl);
        if (!url.isValid() || !url.scheme().startsWith("http")) return;
        QPointer<QListWidget> safeList = list;
        const QPersistentModelIndex itemIndex(list->model()->index(list->row(item), 0));
        artworkLoader->load(url, QSize(64, 64), list,
            [this, safeList, itemIndex](const QImage &image) {
                if (!safeList || !itemIndex.isValid()) return;
                auto *liveItem = safeList->item(itemIndex.row());
                if (!liveItem) return;
                const auto iconPixmap = QPixmap::fromImage(image);
                liveItem->setIcon(QIcon(iconPixmap));
                const bool current = safeList->currentItem() == liveItem;
                if (current && safeList == installedExtensions && installedExtensionDetailIcon) {
                    installedExtensionDetailIcon->setPixmap(iconPixmap);
                } else if (current && safeList == extensions && catalogExtensionDetailIcon) {
                    catalogExtensionDetailIcon->setPixmap(iconPixmap);
                }
            }, CloudStream::ArtworkLoader::LowPriority,
               CloudStream::ArtworkLoader::FitInside);
    }

    void applyExtensionFilter() {
        const auto query = extensionSearch ? extensionSearch->text() : QString();
        const auto filterList = [&query](QListWidget *list) {
            if (!list) return;
            for (int row = 0; row < list->count(); ++row) {
                auto *item = list->item(row);
                item->setHidden(!CloudStream::ExtensionListFilter::matches(
                    item->text() + " " + item->toolTip(), query));
            }
        };
        filterList(repositories);
        filterList(installedExtensions);
        filterList(extensions);
    }

    void refreshRepositoryList() {
        if (!repositories) return;
        const auto selectedUrl = settings.value("selectedRepositoryUrl").toString();
        repositories->blockSignals(true);
        repositories->clear();
        int selectedRow = -1;
        const auto values = extensionRegistry.repositories();
        for (const auto &repository : values) {
            auto *item = new QListWidgetItem(QIcon(":/icons/github.svg"), repository.name + "\n" + repository.url);
            item->setData(Qt::UserRole, repository.url);
            item->setToolTip(repository.url);
            repositories->addItem(item);
            if (repository.url == selectedUrl) selectedRow = repositories->count() - 1;
        }
        if (selectedRow < 0 && repositories->count() > 0) selectedRow = 0;
        repositories->blockSignals(false);
        repositories->setCurrentRow(selectedRow);
        const bool hasSelection = repositories->currentItem() != nullptr;
        if (selectedRepositoryLabel) {
            selectedRepositoryLabel->setText(hasSelection
                ? repositories->currentItem()->text().section('\n', 0, 0) + " selected"
                : "Select a repository");
        }
        if (extensionCatalogContext) {
            extensionCatalogContext->setText(hasSelection
                ? "Browsing " + repositories->currentItem()->text().section('\n', 0, 0)
                : "Choose a repository to browse its catalog");
        }
        if (openRepositoryButton) openRepositoryButton->setEnabled(hasSelection);
        if (removeRepositoryButton) removeRepositoryButton->setEnabled(hasSelection);
        if (extensionManagerTabs) extensionManagerTabs->setTabText(0, "Repositories (" + QString::number(values.size()) + ")");
        applyExtensionFilter();
    }

    void refreshInstalledExtensionsUi() {
        extensionRegistry.synchronizeArtifacts(CloudStream::XdgPaths::extensionsDir());
        if (!installedExtensions) return;
        installedExtensions->clear();
        auto values = extensionRegistry.extensions();
        std::sort(values.begin(), values.end(), [](const auto &left, const auto &right) {
            return left.displayName.localeAwareCompare(right.displayName) < 0;
        });
        for (const auto &extension : values) {
            const auto displayName = extension.displayName.isEmpty() ? extension.internalName : extension.displayName;
            QString state;
            if (extension.platform == "jvm-configurable") {
                state = "Installed • provider settings required • Linux/JVM";
            } else if (extension.platform.startsWith("jvm")) {
                state = extension.enabled ? "Installed • Enabled • Linux/JVM" : "Installed • Disabled • Linux/JVM";
                if (extension.platform == "jvm-converted") state += " • converted from Android";
            } else if (extension.platform == "android-utility") {
                state = "Downloaded • utility/integration extension • no media provider";
            } else if (extension.platform == "android-incompatible") {
                state = "Downloaded • requires Android runtime or UI";
            } else if (extension.platform == "android-failed") {
                state = "Downloaded • Linux conversion failed validation";
            } else {
                state = "Downloaded • awaiting Linux conversion";
            }
            if (extension.version > 0) state += " • v" + QString::number(extension.version);
            auto *item = new QListWidgetItem(QIcon(":/icons/extension.svg"), displayName + "\n" + state);
            item->setData(Qt::UserRole, extension.internalName);
            item->setData(Qt::UserRole + 1, extension.repositoryUrl);
            item->setData(Qt::UserRole + 2, extension.artifactPath);
            item->setData(Qt::UserRole + 3, extension.platform);
            item->setData(Qt::UserRole + 4, extension.enabled);
            item->setToolTip(extension.artifactPath + (extension.repositoryUrl.isEmpty() ? QString() : "\n" + extension.repositoryUrl));
            installedExtensions->addItem(item);
            loadExtensionIcon(installedExtensions, item, extension.iconUrl);
        }
        if (installedExtensions->count() > 0) {
            installedExtensions->setCurrentRow(0);
        } else {
            auto *empty = new QListWidgetItem(QIcon(":/icons/extension.svg"), "No downloaded extensions");
            empty->setFlags(Qt::NoItemFlags);
            installedExtensions->addItem(empty);
        }
        if (extensionManagerTabs) extensionManagerTabs->setTabText(1, "Downloaded (" + QString::number(values.size()) + ")");
        applyExtensionFilter();
    }

    void validateConfiguredExtension(CloudStream::ExtensionRecord record) {
        const auto helper = CloudStream::ProviderHostCommand::discover();
        if (helper.isEmpty()) {
            status->setText("Provider host is not installed");
            return;
        }
        auto *validation = new QProcess(this);
        helper.configure(validation, {"list", record.artifactPath, "auto"});
        status->setText("Validating configured provider…");
        CloudStream::ProcessCompletion::watch(validation, this,
                [this, validation, record](int exitCode, QProcess::ExitStatus, bool) mutable {
            const auto providers = QJsonDocument::fromJson(validation->readAllStandardOutput()).array();
            const auto diagnostic = QString::fromUtf8(validation->readAllStandardError()).trimmed();
            const auto result = CloudStream::ProviderValidation::classify(
                exitCode, providers.size(), diagnostic);
            validation->deleteLater();
            if (result != CloudStream::ProviderValidationResult::Runnable) {
                record.platform = "jvm-configurable";
                record.enabled = false;
                extensionRegistry.upsertExtension(record);
                refreshInstalledExtensionsUi();
                const auto message = diagnostic.isEmpty()
                    ? "The saved settings registered no media providers. Check the URL and credentials."
                    : diagnostic.section('\n', 0, 0);
                status->setText("Provider settings need attention");
                QMessageBox::warning(this, "Provider validation failed", message);
                return;
            }
            record.platform = record.sourceArtifactPath.isEmpty() ? "jvm" : "jvm-converted";
            record.enabled = true;
            extensionRegistry.upsertExtension(record);
            refreshInstalledExtensionsUi();
            refreshProviderChoices();
            status->setText("Configured " + record.displayName + " • " +
                            QString::number(providers.size()) + " provider(s) ready");
            QMessageBox::information(this, "Provider ready",
                record.displayName + " registered " + QString::number(providers.size()) +
                " provider(s) and is now enabled.");
        });
        validation->start();
        QTimer::singleShot(30000, validation, [validation] {
            if (validation->state() != QProcess::NotRunning) validation->kill();
        });
    }

    void configureSelectedExtension() {
        auto *item = installedExtensions ? installedExtensions->currentItem() : nullptr;
        if (!item || item->data(Qt::UserRole).toString().isEmpty()) return;
        const auto internalName = item->data(Qt::UserRole).toString();
        const auto repositoryUrl = item->data(Qt::UserRole + 1).toString();
        const auto records = extensionRegistry.extensions();
        const auto found = std::find_if(records.begin(), records.end(),
            [&](const auto &record) {
                return record.internalName == internalName && record.repositoryUrl == repositoryUrl;
            });
        if (found == records.end()) return;
        const auto record = *found;
        const auto spec = CloudStream::ProviderConfiguration::specFor(record.internalName);
        if (!spec) {
            QMessageBox::information(this, "No desktop settings",
                "This extension does not expose a supported native configuration schema.");
            return;
        }

        CloudStream::ProviderConfigurationInput initial;
        initial.name = record.displayName.isEmpty() ? record.internalName : record.displayName;
        if (!spec->typeOptions.isEmpty()) initial.type = spec->typeOptions.first();
        QString existingWarning;
        QFile existingFile(CloudStream::ProviderConfiguration::settingsPath(record.artifactPath));
        if (existingFile.exists() && existingFile.open(QIODevice::ReadOnly)) {
            const auto document = QJsonDocument::fromJson(existingFile.readAll());
            const auto decoded = document.isObject()
                ? CloudStream::ProviderConfiguration::inputFromSidecar(
                    record.internalName, document.object(), &existingWarning)
                : std::optional<CloudStream::ProviderConfigurationInput>();
            if (decoded) initial = *decoded;
            else if (existingWarning.isEmpty()) existingWarning = "Existing settings are malformed and will be replaced.";
        }

        QWidget *configurationParent = extensionManagerDialog
            ? static_cast<QWidget *>(extensionManagerDialog)
            : static_cast<QWidget *>(this);
        QDialog dialog(configurationParent);
        dialog.setObjectName("providerConfigurationDialog");
        dialog.setWindowTitle("Configure " + initial.name);
        dialog.setMinimumSize(640, 440);
        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(24, 22, 24, 22);
        layout->setSpacing(14);
        auto *heading = new QHBoxLayout;
        auto *icon = new QLabel;
        icon->setPixmap(item->icon().pixmap(QSize(42, 42)));
        heading->addWidget(icon);
        heading->addWidget(title("Provider settings", 22));
        heading->addStretch();
        layout->addLayout(heading);
        auto *explanation = new QLabel(
            "These desktop settings replace the extension's Android-only settings screen. "
            "Playlist and account URLs may contain credentials. Values are stored locally as plaintext "
            "with owner-only file permissions; do not share the settings file.");
        explanation->setObjectName("muted");
        explanation->setWordWrap(true);
        layout->addWidget(explanation);
        auto *message = new QLabel(existingWarning);
        message->setObjectName("configurationError");
        message->setWordWrap(true);
        message->setVisible(!existingWarning.isEmpty());
        layout->addWidget(message);

        auto *form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setHorizontalSpacing(18);
        form->setVerticalSpacing(12);
        auto *name = new QLineEdit(initial.name);
        name->setClearButtonEnabled(false);
        name->setAccessibleName("Display name");
        name->setAccessibleDescription("Required");
        form->addRow("Display name (required)", name);
        auto *primaryUrl = new QLineEdit(initial.primaryUrl);
        primaryUrl->setPlaceholderText("https://");
        primaryUrl->setClearButtonEnabled(true);
        primaryUrl->setAccessibleName(spec->primaryUrlLabel);
        primaryUrl->setAccessibleDescription("Required HTTP or HTTPS URL");
        form->addRow(spec->primaryUrlLabel + " (required)", primaryUrl);
        QLineEdit *secondaryUrl = nullptr;
        if (!spec->secondaryUrlLabel.isEmpty()) {
            secondaryUrl = new QLineEdit(initial.secondaryUrl);
            secondaryUrl->setPlaceholderText("https://");
            secondaryUrl->setClearButtonEnabled(true);
            form->addRow(spec->secondaryUrlLabel + " (optional)", secondaryUrl);
        }
        QLineEdit *username = nullptr;
        QLineEdit *password = nullptr;
        if (spec->kind == CloudStream::ProviderConfigurationKind::Xtream) {
            username = new QLineEdit(initial.username);
            username->setClearButtonEnabled(true);
            password = new QLineEdit(initial.password);
            password->setEchoMode(QLineEdit::Password);
            password->setClearButtonEnabled(true);
            username->setAccessibleDescription("Required");
            password->setAccessibleDescription("Required; stored locally as plaintext with owner-only file permissions");
            form->addRow("Username (required)", username);
            form->addRow("Password (required)", password);
        }
        QComboBox *type = nullptr;
        if (!spec->typeOptions.isEmpty()) {
            type = new QComboBox;
            type->addItems(spec->typeOptions);
            const auto selected = type->findText(initial.type);
            type->setCurrentIndex(selected < 0 ? 0 : selected);
            form->addRow("Provider type", type);
        }
        layout->addLayout(form);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        auto *saveButton = buttons->button(QDialogButtonBox::Save);
        saveButton->setText("Save and validate");
        saveButton->setProperty("primary", true);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);

        const auto currentInput = [name, primaryUrl, secondaryUrl, username, password, type] {
            CloudStream::ProviderConfigurationInput input;
            input.name = name->text();
            input.primaryUrl = primaryUrl->text();
            if (secondaryUrl) input.secondaryUrl = secondaryUrl->text();
            if (username) input.username = username->text();
            if (password) input.password = password->text();
            if (type) input.type = type->currentText();
            return input;
        };
        const auto validateForm = [record, currentInput, message, saveButton, primaryUrl,
                                   primaryLabel = spec->primaryUrlLabel, existingWarning] {
            QString error;
            const auto candidate = CloudStream::ProviderConfiguration::sidecar(
                record.internalName, record.repositoryUrl, record.version, currentInput(), &error);
            saveButton->setEnabled(!candidate.isEmpty());
            primaryUrl->setProperty("invalid", candidate.isEmpty() && error.startsWith(primaryLabel));
            primaryUrl->style()->unpolish(primaryUrl);
            primaryUrl->style()->polish(primaryUrl);
            message->setText(candidate.isEmpty() ? error : existingWarning);
            message->setVisible(!message->text().isEmpty());
        };
        connect(name, &QLineEdit::textChanged, &dialog, [validateForm] { validateForm(); });
        connect(primaryUrl, &QLineEdit::textChanged, &dialog, [validateForm] { validateForm(); });
        if (secondaryUrl) connect(secondaryUrl, &QLineEdit::textChanged, &dialog, [validateForm] { validateForm(); });
        if (username) connect(username, &QLineEdit::textChanged, &dialog, [validateForm] { validateForm(); });
        if (password) connect(password, &QLineEdit::textChanged, &dialog, [validateForm] { validateForm(); });
        if (type) connect(type, &QComboBox::currentTextChanged, &dialog, [validateForm] { validateForm(); });
        validateForm();

        const auto configurationPreviewPath = providerConfigurationPreviewOutputPath;
        if (!configurationPreviewPath.isEmpty()) {
            QTimer::singleShot(400, &dialog, [&dialog, configurationPreviewPath] {
                dialog.grab().save(configurationPreviewPath);
                dialog.reject();
            });
        }
        while (dialog.exec() == QDialog::Accepted) {
            const auto input = currentInput();
            QString error;
            const auto sidecar = CloudStream::ProviderConfiguration::sidecar(
                record.internalName, record.repositoryUrl, record.version, input, &error);
            if (sidecar.isEmpty() || !CloudStream::ProviderConfiguration::writeSidecar(
                    record.artifactPath, sidecar, &error)) {
                message->setText(error);
                message->show();
                continue;
            }
            validateConfiguredExtension(record);
            break;
        }
        if (!configurationPreviewPath.isEmpty()) providerConfigurationPreviewOutputPath.clear();
    }

    void toggleSelectedExtension() {
        auto *item = installedExtensions ? installedExtensions->currentItem() : nullptr;
        if (!item || item->data(Qt::UserRole).toString().isEmpty()) return;
        const auto platform = item->data(Qt::UserRole + 3).toString();
        if (platform == "jvm-configurable") {
            configureSelectedExtension();
            return;
        }
        if (!platform.startsWith("jvm")) {
            if (platform == "android-utility") {
                QMessageBox::information(this, "Utility extension",
                    "This extension manages repositories rather than media providers. Use Run repository utility to import its repository documents safely on Linux.");
            } else {
                QMessageBox::information(this, "Conversion required",
                    "This Android package has not completed Linux conversion yet. Refresh to retry it.");
            }
            return;
        }
        const auto enabled = !item->data(Qt::UserRole + 4).toBool();
        if (extensionRegistry.setExtensionEnabled(item->data(Qt::UserRole).toString(),
                item->data(Qt::UserRole + 1).toString(), enabled)) {
            status->setText(enabled ? "Extension enabled" : "Extension disabled");
            refreshInstalledExtensionsUi();
            refreshProviderChoices();
        }
    }

    void runSelectedRepositoryUtility() {
        auto *item = installedExtensions ? installedExtensions->currentItem() : nullptr;
        if (!item || !item->data(Qt::UserRole + 3).toString().contains("utility")) {
            status->setText("Select a downloaded repository utility");
            return;
        }
        const auto helper = CloudStream::ProviderHostCommand::discover();
        if (helper.isEmpty()) {
            status->setText("Provider host is not installed");
            return;
        }
        const auto artifact = item->data(Qt::UserRole + 2).toString();
        auto *inspection = new QProcess(this);
        helper.configure(inspection, {"repository-candidates", artifact});
        status->setText("Inspecting repository utility without executing it…");
        CloudStream::ProcessCompletion::watch(inspection, this,
                [this, inspection](int exitCode, QProcess::ExitStatus, bool) {
            const auto document = QJsonDocument::fromJson(inspection->readAllStandardOutput());
            QStringList candidates;
            if (document.isArray()) {
                for (const auto &value : document.array()) {
                    if (value.isString() && !value.toString().isEmpty()) candidates << value.toString();
                }
            }
            if (exitCode != 0 || candidates.isEmpty()) {
                status->setText("Repository utility contains no supported repository document");
                QMessageBox::information(this, "No repository document found",
                    "This utility cannot be translated safely to a native Linux repository action.");
            } else {
                status->setText("Found " + QString::number(candidates.size()) + " repository document(s)");
                for (const auto &candidate : candidates) importRepositoryIndexFromUrl(candidate);
            }
            inspection->deleteLater();
        });
        inspection->start();
        QTimer::singleShot(30000, inspection, [inspection] {
            if (inspection->state() != QProcess::NotRunning) inspection->kill();
        });
    }

    void removeSelectedExtension() {
        auto *item = installedExtensions ? installedExtensions->currentItem() : nullptr;
        if (!item || item->data(Qt::UserRole).toString().isEmpty()) return;
        const auto name = item->text().section('\n', 0, 0);
        if (QMessageBox::question(this, "Remove extension", "Remove " + name + " and its downloaded file?") != QMessageBox::Yes) return;
        const auto path = item->data(Qt::UserRole + 2).toString();
        if (QFileInfo::exists(path) && !QFile::remove(path)) {
            status->setText("Could not remove extension file");
            return;
        }
        for (const auto &extension : extensionRegistry.extensions()) {
            if (extension.internalName == item->data(Qt::UserRole).toString() &&
                extension.repositoryUrl == item->data(Qt::UserRole + 1).toString() &&
                !extension.sourceArtifactPath.isEmpty() && extension.sourceArtifactPath != path) {
                QFile::remove(extension.sourceArtifactPath);
            }
        }
        extensionRegistry.removeExtension(item->data(Qt::UserRole).toString(), item->data(Qt::UserRole + 1).toString());
        refreshInstalledExtensionsUi();
        refreshProviderChoices();
        status->setText("Removed " + name);
    }

    void useSelectedExtensionOnHome() {
        auto *item = installedExtensions ? installedExtensions->currentItem() : nullptr;
        if (!item || !item->data(Qt::UserRole + 3).toString().startsWith("jvm") || !item->data(Qt::UserRole + 4).toBool()) {
            status->setText("Select an enabled Linux/JVM extension");
            return;
        }
        const auto path = item->data(Qt::UserRole + 2).toString();
        for (int index = 0; homeProviderSelector && index < homeProviderSelector->count(); ++index) {
            const auto provider = QJsonObject::fromVariantMap(homeProviderSelector->itemData(index).toMap());
            if (provider.value("jarPath").toString() == path) {
                homeProviderSelector->setCurrentIndex(index);
                pages->setCurrentIndex(0);
                return;
            }
        }
        status->setText("That extension registered no Home provider");
    }

    void removeSelectedRepository() {
        const auto repositoryUrl = currentRepositoryUrl();
        if (repositoryUrl.isEmpty()) return;
        if (QMessageBox::question(this, "Remove repository",
                "Remove this repository and extensions downloaded from it?") != QMessageBox::Yes) return;
        for (const auto &extension : extensionRegistry.extensions()) {
            if (extension.repositoryUrl != repositoryUrl) continue;
            if (QFileInfo::exists(extension.artifactPath)) QFile::remove(extension.artifactPath);
            if (!extension.sourceArtifactPath.isEmpty() && extension.sourceArtifactPath != extension.artifactPath) QFile::remove(extension.sourceArtifactPath);
        }
        if (extensionRegistry.removeRepository(repositoryUrl)) {
            settings.remove("selectedRepositoryUrl");
            refreshRepositoryList();
            refreshInstalledExtensionsUi();
            refreshProviderChoices();
            status->setText("Repository removed");
        }
    }

    QWidget *extensionManagerPanel() {
        auto *panel = new QWidget;
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        extensionSearch = new QLineEdit;
        extensionSearch->setObjectName("searchField");
        extensionSearch->setPlaceholderText("Search repositories and extensions…");
        extensionSearch->setClearButtonEnabled(true);
        extensionSearch->addAction(QIcon(":/icons/search.svg"), QLineEdit::LeadingPosition);
        extensionSearch->setMinimumHeight(48);
        layout->addWidget(extensionSearch);

        extensionManagerTabs = new QTabBar;
        extensionManagerTabs->setObjectName("extensionTabs");
        extensionManagerTabs->setDrawBase(false);
        extensionManagerTabs->setExpanding(false);
        extensionManagerTabs->addTab("Repositories");
        extensionManagerTabs->addTab("Downloaded");
        extensionManagerTabs->addTab("Available");
        layout->addWidget(extensionManagerTabs);

        auto *stack = new QStackedWidget;
        layout->addWidget(stack, 1);

        auto *repoPage = new QWidget;
        auto *rv = new QVBoxLayout(repoPage);
        rv->setContentsMargins(0, 0, 0, 0);
        rv->setSpacing(10);
        auto *info = new QLabel("Add one repository, or import a repository index such as Mega's community list.");
        info->setObjectName("muted");
        info->setWordWrap(true);
        rv->addWidget(info);
        auto *actions = new QHBoxLayout;
        auto *add = button("Add repository", true);
        add->setIcon(QIcon(":/icons/extension-dark.svg"));
        auto *importIndex = button("Import repository index");
        importIndex->setIcon(QIcon(":/icons/download.svg"));
        actions->addWidget(add);
        actions->addWidget(importIndex);
        actions->addStretch();
        rv->addLayout(actions);
        repositories = new QListWidget;
        repositories->setObjectName("extensionList");
        repositories->setIconSize(QSize(34, 34));
        rv->addWidget(repositories, 1);
        auto *repoSelection = new QWidget;
        repoSelection->setObjectName("selectionBar");
        auto *repoSelectionLayout = new QHBoxLayout(repoSelection);
        repoSelectionLayout->setContentsMargins(14, 8, 10, 8);
        selectedRepositoryLabel = new QLabel("Select a repository");
        selectedRepositoryLabel->setObjectName("selectedRepositoryLabel");
        repoSelectionLayout->addWidget(selectedRepositoryLabel);
        repoSelectionLayout->addStretch();
        auto *open = openRepositoryButton = button("Open");
        open->setIcon(QIcon(":/icons/github.svg"));
        auto *remove = removeRepositoryButton = button("Remove");
        remove->setIcon(QIcon(":/icons/delete.svg"));
        remove->setProperty("destructive", true);
        open->setEnabled(false);
        remove->setEnabled(false);
        repoSelectionLayout->addWidget(open);
        repoSelectionLayout->addWidget(remove);
        rv->addWidget(repoSelection);
        connect(add, &QPushButton::clicked, this, [this] { addRepository(); });
        connect(importIndex, &QPushButton::clicked, this, [this] {
            bool ok = false;
            const auto url = QInputDialog::getText(this, "Import repository index", "Repository index URL:",
                QLineEdit::Normal,
                "https://raw.githubusercontent.com/recloudstream/cs-repos/master/repos-db.json", &ok).trimmed();
            if (ok && !url.isEmpty()) importRepositoryIndexFromUrl(url);
        });
        connect(open, &QPushButton::clicked, this, [this] { if (!currentRepositoryUrl().isEmpty()) QDesktopServices::openUrl(QUrl(currentRepositoryUrl())); });
        connect(remove, &QPushButton::clicked, this, [this] { removeSelectedRepository(); });
        connect(repositories, &QListWidget::currentRowChanged, this, [this] {
            auto *item = repositories ? repositories->currentItem() : nullptr;
            const auto repositoryUrl = currentRepositoryUrl();
            if (selectedRepositoryLabel) {
                selectedRepositoryLabel->setText(item ? item->text().section('\n', 0, 0) + " selected"
                                                       : "Select a repository");
            }
            if (repositoryUrl.isEmpty()) return;
            settings.setValue("selectedRepositoryUrl", repositoryUrl);
            loadExtensions();
        });
        connect(repositories, &QListWidget::currentItemChanged, this,
                [open, remove](QListWidgetItem *item) {
            const bool valid = item && !item->data(Qt::UserRole).toString().isEmpty();
            open->setEnabled(valid);
            remove->setEnabled(valid);
        });
        connect(repositories, &QListWidget::itemActivated, this, [this](QListWidgetItem *) {
            if (extensionManagerTabs) extensionManagerTabs->setCurrentIndex(2);
        });
        stack->addWidget(repoPage);

        auto *installedPage = new QWidget;
        auto *installedLayout = new QVBoxLayout(installedPage);
        installedLayout->setContentsMargins(0, 0, 0, 0);
        installedLayout->setSpacing(10);
        auto *installedInfo = new QLabel("Native JVM extensions load directly. Android .cs3 packages are converted out of process, validated, and then exposed as Linux providers; Android-only settings UI is ignored.");
        installedInfo->setWordWrap(true);
        installedInfo->setObjectName("muted");
        auto *installedHeader = new QHBoxLayout;
        installedHeader->addWidget(installedInfo, 1);
        auto *installedRefresh = button("Refresh");
        installedRefresh->setIcon(QIcon(":/icons/refresh.svg"));
        installedHeader->addWidget(installedRefresh, 0, Qt::AlignTop);
        installedLayout->addLayout(installedHeader);
        auto *installedBody = new QHBoxLayout;
        installedBody->setSpacing(14);
        installedExtensions = new QListWidget;
        installedExtensions->setObjectName("extensionList");
        installedExtensions->setIconSize(QSize(34, 34));
        installedBody->addWidget(installedExtensions, 1);
        auto *installedDetails = new QWidget;
        installedDetails->setObjectName("extensionDetailPanel");
        installedDetails->setFixedWidth(290);
        auto *installedActions = new QVBoxLayout(installedDetails);
        installedActions->setContentsMargins(18, 18, 18, 18);
        installedActions->setSpacing(10);
        installedExtensionDetailIcon = new QLabel;
        installedExtensionDetailIcon->setPixmap(QIcon(":/icons/extension.svg").pixmap(QSize(56, 56)));
        installedActions->addWidget(installedExtensionDetailIcon, 0, Qt::AlignLeft);
        auto *installedName = title("Select an extension", 19);
        installedName->setWordWrap(true);
        auto *installedStatus = new QLabel("Choose a downloaded extension to manage it.");
        installedStatus->setObjectName("muted");
        installedStatus->setWordWrap(true);
        installedActions->addWidget(installedName);
        installedActions->addWidget(installedStatus);
        installedActions->addStretch();
        auto *configure = button("Configure", true);
        configure->setIcon(QIcon(":/icons/settings-outline.svg"));
        auto *toggle = button("Enable / disable");
        toggle->setEnabled(false);
        auto *runUtility = button("Run repository utility");
        auto *useHome = button("Use on Home", true);
        useHome->setIcon(QIcon(":/icons/home-outline.svg"));
        auto *removeInstalled = button("Remove");
        removeInstalled->setIcon(QIcon(":/icons/delete.svg"));
        removeInstalled->setProperty("destructive", true);
        removeInstalled->setEnabled(false);
        installedActions->addWidget(configure);
        installedActions->addWidget(useHome);
        installedActions->addWidget(toggle);
        installedActions->addWidget(runUtility);
        installedActions->addWidget(removeInstalled);
        installedBody->addWidget(installedDetails);
        installedLayout->addLayout(installedBody, 1);
        connect(installedRefresh, &QPushButton::clicked, this, [this] {
            refreshInstalledExtensionsUi();
            convertDownloadedAndroidExtensions();
            refreshProviderChoices();
        });
        connect(configure, &QPushButton::clicked, this, [this] { configureSelectedExtension(); });
        connect(toggle, &QPushButton::clicked, this, [this] { toggleSelectedExtension(); });
        connect(runUtility, &QPushButton::clicked, this, [this] { runSelectedRepositoryUtility(); });
        connect(useHome, &QPushButton::clicked, this, [this] { useSelectedExtensionOnHome(); });
        connect(removeInstalled, &QPushButton::clicked, this, [this] { removeSelectedExtension(); });
        connect(installedExtensions, &QListWidget::currentItemChanged, this,
                [this, installedName, installedStatus, configure, toggle, runUtility, useHome, removeInstalled](QListWidgetItem *item) {
            const auto platform = item ? item->data(Qt::UserRole + 3).toString() : QString();
            const bool valid = item && !item->data(Qt::UserRole).toString().isEmpty();
            const bool enabled = valid && item->data(Qt::UserRole + 4).toBool();
            const auto internalName = valid ? item->data(Qt::UserRole).toString() : QString();
            installedExtensionDetailIcon->setPixmap(valid
                ? item->icon().pixmap(QSize(56, 56))
                : QIcon(":/icons/extension.svg").pixmap(QSize(56, 56)));
            installedName->setText(valid ? item->text().section('\n', 0, 0) : "Select an extension");
            installedStatus->setText(valid ? item->text().section('\n', 1) : "Choose a downloaded extension to manage it.");
            configure->setVisible(valid && CloudStream::ProviderConfiguration::specFor(internalName).has_value());
            toggle->setText(enabled ? "Disable" : "Enable");
            toggle->setVisible(valid && platform.startsWith("jvm") && platform != "jvm-configurable");
            toggle->setEnabled(valid);
            runUtility->setVisible(platform.contains("utility"));
            bool onHome = false;
            if (valid && homeProviderSelector) {
                const auto selected = QJsonObject::fromVariantMap(homeProviderSelector->currentData().toMap());
                onHome = selected.value("jarPath").toString() == item->data(Qt::UserRole + 2).toString();
            }
            useHome->setText(onHome ? "Using on Home" : "Use on Home");
            useHome->setEnabled(!onHome);
            useHome->setVisible(platform.startsWith("jvm") && enabled);
            removeInstalled->setEnabled(valid);
        });
        configure->hide();
        toggle->hide();
        runUtility->hide();
        useHome->hide();
        stack->addWidget(installedPage);

        auto *catalogPage = new QWidget;
        auto *ev = new QVBoxLayout(catalogPage);
        ev->setContentsMargins(0, 0, 0, 0);
        ev->setSpacing(10);
        auto *catalogHeader = new QHBoxLayout;
        extensionCatalogContext = new QLabel("Choose a repository to browse its catalog");
        extensionCatalogContext->setObjectName("muted");
        extensionCatalogContext->setWordWrap(true);
        catalogHeader->addWidget(extensionCatalogContext, 1);
        auto *refresh = button("Refresh");
        refresh->setIcon(QIcon(":/icons/refresh.svg"));
        auto *installAll = installAllExtensionsButton = button("Install all", true);
        installAll->setIcon(QIcon(":/icons/download-dark.svg"));
        installAll->setEnabled(false);
        auto *folder = button("Open extensions folder");
        folder->setIcon(QIcon(":/icons/library-outline.svg"));
        catalogHeader->addWidget(refresh, 0, Qt::AlignTop);
        catalogHeader->addWidget(installAll, 0, Qt::AlignTop);
        catalogHeader->addWidget(folder, 0, Qt::AlignTop);
        ev->addLayout(catalogHeader);
        auto *catalogBody = new QHBoxLayout;
        catalogBody->setSpacing(14);
        extensions = new QListWidget;
        extensions->setObjectName("extensionList");
        extensions->setIconSize(QSize(34, 34));
        extensions->addItem("Select a repository to browse extensions");
        catalogBody->addWidget(extensions, 1);
        auto *catalogDetails = new QWidget;
        catalogDetails->setObjectName("extensionDetailPanel");
        catalogDetails->setFixedWidth(290);
        auto *extensionActions = new QVBoxLayout(catalogDetails);
        extensionActions->setContentsMargins(18, 18, 18, 18);
        extensionActions->setSpacing(10);
        catalogExtensionDetailIcon = new QLabel;
        catalogExtensionDetailIcon->setPixmap(QIcon(":/icons/extension.svg").pixmap(QSize(56, 56)));
        extensionActions->addWidget(catalogExtensionDetailIcon, 0, Qt::AlignLeft);
        auto *catalogName = title("Select an extension", 19);
        catalogName->setWordWrap(true);
        auto *catalogStatus = new QLabel("Choose an extension to see its install state.");
        catalogStatus->setObjectName("muted");
        catalogStatus->setWordWrap(true);
        auto *catalogDescription = new QLabel;
        catalogDescription->setObjectName("muted");
        catalogDescription->setWordWrap(true);
        extensionActions->addWidget(catalogName);
        extensionActions->addWidget(catalogStatus);
        extensionActions->addWidget(catalogDescription);
        extensionActions->addStretch();
        auto *install = button("Install", true);
        install->setIcon(QIcon(":/icons/download.svg"));
        install->setEnabled(false);
        extensionActions->addWidget(install);
        catalogBody->addWidget(catalogDetails);
        ev->addLayout(catalogBody, 1);
        connect(refresh, &QPushButton::clicked, this, [this] { loadExtensions(); });
        connect(installAll, &QPushButton::clicked, this, [this] { installAllExtensions(); });
        connect(install, &QPushButton::clicked, this, [this] { downloadSelectedExtension(); });
        connect(folder, &QPushButton::clicked, this, [this] {
            showInAppFolderDialog(CloudStream::XdgPaths::extensionsDir(), "Extensions", false, {});
        });
        connect(extensions, &QListWidget::currentItemChanged, this,
                [this, catalogName, catalogStatus, catalogDescription, install](QListWidgetItem *item) {
            const bool valid = item && item->data(Qt::UserRole).isValid();
            catalogExtensionDetailIcon->setPixmap(valid
                ? item->icon().pixmap(QSize(56, 56))
                : QIcon(":/icons/extension.svg").pixmap(QSize(56, 56)));
            catalogName->setText(valid ? item->text().section('\n', 0, 0) : "Select an extension");
            catalogStatus->setText(valid ? item->text().section('\n', 1)
                                         : "Choose an extension to see its install state.");
            catalogDescription->setText(valid ? item->toolTip().section('\n', 0, 0) : QString());
            const bool installed = valid && item->text().section('\n', 1).startsWith("Installed");
            install->setText(installed ? "Update / reinstall" : "Install");
            install->setEnabled(valid);
        });
        stack->addWidget(catalogPage);

        connect(extensionManagerTabs, &QTabBar::currentChanged, stack, &QStackedWidget::setCurrentIndex);
        connect(extensionSearch, &QLineEdit::textChanged, this, [this] { applyExtensionFilter(); });
        refreshRepositoryList();
        refreshInstalledExtensionsUi();
        if (!currentRepositoryUrl().isEmpty()) loadExtensions();
        return panel;
    }

    void showExtensionManager() {
        if (!extensionManagerDialog) {
            extensionManagerDialog = new QDialog(this);
            extensionManagerDialog->setObjectName("extensionManagerDialog");
            extensionManagerDialog->setWindowTitle("CloudStream Extensions");
            extensionManagerDialog->resize(1060, 760);
            extensionManagerDialog->setMinimumSize(900, 640);
            auto *dialogLayout = new QVBoxLayout(extensionManagerDialog);
            dialogLayout->setContentsMargins(22, 18, 22, 18);
            auto *header = new QHBoxLayout;
            auto *icon = new QLabel;
            icon->setPixmap(QIcon(":/icons/extension.svg").pixmap(QSize(32, 32)));
            header->addWidget(icon);
            header->addWidget(title("Extensions", 23));
            header->addStretch();
            auto *close = button("Close");
            header->addWidget(close);
            dialogLayout->addLayout(header);
            dialogLayout->addWidget(extensionManagerPanel(), 1);
            connect(close, &QPushButton::clicked, extensionManagerDialog, &QDialog::hide);
        }
        refreshRepositoryList();
        refreshInstalledExtensionsUi();
        CloudStream::SmoothScrollController::attachRecursively(extensionManagerDialog);
        if (settings.value("interface/windowMode", "Separate windows") == "Single-window navigation") {
            extensionManagerDialog->setParent(centralWidget());
            extensionManagerDialog->setWindowFlags(Qt::Widget);
            extensionManagerDialog->setGeometry(centralWidget()->rect());
        } else {
            extensionManagerDialog->setParent(this);
            extensionManagerDialog->setWindowFlags(Qt::Dialog);
        }
        extensionManagerDialog->show();
        extensionManagerDialog->raise();
        extensionManagerDialog->activateWindow();
    }

    QWidget *settingsPage() {
        settingsPane = new CloudStream::SettingsPane(&settings);
        connect(settingsPane, &CloudStream::SettingsPane::extensionsRequested,
                this, &CloudStreamWindow::showExtensionManager);
        connect(settingsPane, &CloudStream::SettingsPane::homeProviderRequested, this, [this] {
            pages->setCurrentIndex(0);
            showHomeProviderDialog();
        });
        connect(settingsPane, &CloudStream::SettingsPane::searchProvidersRequested, this, [this] {
            showSearchProviderDialog();
        });
        connect(settingsPane, &CloudStream::SettingsPane::statusMessage, this,
                [this](const QString &message) { status->setText(message); });
        connect(settingsPane, &CloudStream::SettingsPane::settingChanged, this,
                [this](const QString &key) {
            if (key.startsWith("providers/")) finishProviderChoices(allProviderChoices, true);
            if (key == "downloadFolder" && downloadsRefresh) downloadsRefresh();
            if (key.startsWith("interface/")) applyAppearance();
        });
        return settingsPane;
    }

    QString repositoryManifestUrl(const QString &value) const {
        return CloudStream::RepositoryUrlResolver::manifestUrl(value);
    }

    void refreshInstallAllButton() {
        if (!installAllExtensionsButton) return;
        const auto pending = CloudStream::ExtensionInstallBatch::pending(
            currentCatalogPlugins, extensionRegistry.extensions(), currentCatalogRepositoryUrl);
        installAllExtensionsButton->setText(pending.isEmpty()
            ? "Install all"
            : "Install all (" + QString::number(pending.size()) + ")");
        installAllExtensionsButton->setToolTip(pending.isEmpty()
            ? "Every installable extension in this repository is current"
            : "Install or update every pending extension in this repository");
        installAllExtensionsButton->setEnabled(!bulkExtensionInstallActive && !pending.isEmpty());
    }

    void populateExtensionCatalog(const QList<CloudStream::PluginInfo> &plugins,
                                  const QString &repositoryUrl) {
        currentCatalogPlugins = plugins;
        currentCatalogRepositoryUrl = repositoryUrl;
        extensions->clear();
        for (int pluginIndex = 0; pluginIndex < plugins.size(); ++pluginIndex) {
            const auto &plugin = plugins[pluginIndex];
            auto installed = extensionRegistry.extensions();
            auto record = std::find_if(installed.begin(), installed.end(), [&](const auto &extension) {
                return extension.internalName == plugin.internalName &&
                    (extension.repositoryUrl == repositoryUrl || extension.repositoryUrl.isEmpty());
            });
            if (record != installed.end() && record->repositoryUrl.isEmpty() && QFileInfo::exists(record->artifactPath)) {
                auto adopted = *record;
                extensionRegistry.removeExtension(adopted.internalName, {});
                adopted.displayName = plugin.name;
                adopted.repositoryUrl = repositoryUrl;
                adopted.version = plugin.version;
                adopted.language = plugin.language;
                adopted.tvTypes = plugin.tvTypes;
                extensionRegistry.upsertExtension(adopted);
                installed = extensionRegistry.extensions();
                record = std::find_if(installed.begin(), installed.end(), [&](const auto &extension) {
                    return extension.internalName == plugin.internalName && extension.repositoryUrl == repositoryUrl;
                });
            }
            if (record != installed.end() && !plugin.iconUrl.isEmpty() && record->iconUrl != plugin.iconUrl) {
                record->iconUrl = plugin.iconUrl;
                extensionRegistry.upsertExtension(*record);
            }
            const bool isInstalled = record != installed.end() && QFileInfo::exists(record->artifactPath);
            const bool linuxCapable = !plugin.jarUrl.isEmpty();
            QString state;
            if (isInstalled) {
                if (record->platform.startsWith("jvm")) state = "Installed • Linux/JVM";
                else if (record->platform == "android-utility") state = "Installed • Android repository utility";
                else state = "Saved • awaiting Linux conversion";
            }
            else state = linuxCapable ? "Available • native Linux/JVM" : "Available • Android package • converts on Linux";
            if (plugin.version > 0) state += " • v" + QString::number(plugin.version);
            if (plugin.status == 0) state += " • Provider marked down";
            auto *item = new QListWidgetItem(QIcon(":/icons/extension.svg"), plugin.name + "\n" + state);
            item->setData(Qt::UserRole, plugin.url);
            item->setData(Qt::UserRole + 1, plugin.fileHash);
            item->setData(Qt::UserRole + 2, plugin.internalName);
            item->setData(Qt::UserRole + 3, plugin.jarUrl);
            item->setData(Qt::UserRole + 4, plugin.jarHash);
            item->setData(Qt::UserRole + 5, plugin.name);
            item->setData(Qt::UserRole + 6, plugin.version);
            item->setData(Qt::UserRole + 7, plugin.language);
            item->setData(Qt::UserRole + 8, plugin.tvTypes);
            item->setData(Qt::UserRole + 9, repositoryUrl);
            item->setData(Qt::UserRole + 10, plugin.iconUrl);
            item->setData(Qt::UserRole + 11, pluginIndex);
            item->setToolTip((plugin.description.isEmpty() ? "No description available" : plugin.description) +
                (linuxCapable ? "\nNative Linux/JVM artifact" : "\nAndroid DEX package; CloudStream Linux converts it to JVM on install"));
            extensions->addItem(item);
            loadExtensionIcon(extensions, item, plugin.iconUrl);
        }
        if (extensions->count() == 0) extensions->addItem("No extensions found");
        else extensions->setCurrentRow(0);
        if (extensionManagerTabs) extensionManagerTabs->setTabText(2, "Available (" + QString::number(plugins.size()) + ")");
        applyExtensionFilter();
        refreshInstalledExtensionsUi();
        refreshInstallAllButton();
    }

    void loadExtensions() {
        const auto repositoryUrl = currentRepositoryUrl();
        if (repositoryUrl.isEmpty()) { if (status) status->setText("Select a repository first"); return; }
        currentCatalogPlugins.clear();
        currentCatalogRepositoryUrl = repositoryUrl;
        refreshInstallAllButton();
        extensions->clear(); extensions->addItem("Loading repository manifest…"); status->setText("Loading repository");
        auto request = cloudStreamRequest(QUrl(repositoryManifestUrl(repositoryUrl)));
        auto *manifestReply = network.get(request);
        connect(manifestReply, &QNetworkReply::finished, this, [this, manifestReply, repositoryUrl] {
            if (manifestReply->error() != QNetworkReply::NoError) { extensions->clear(); extensions->addItem("Repository error: " + manifestReply->errorString()); status->setText("Repository unavailable"); manifestReply->deleteLater(); return; }
            CloudStream::RepositoryManifest manifest;
            QString parseError;
            if (!CloudStream::RepositoryManifestParser::parseManifest(manifestReply->readAll(), &manifest, &parseError)) {
                extensions->clear(); extensions->addItem("Invalid repository: " + parseError); status->setText("Repository manifest rejected"); manifestReply->deleteLater(); return;
            }
            extensionRegistry.addRepository({manifest.name, repositoryUrl});
            if (repositories && repositories->currentItem()) repositories->currentItem()->setText(manifest.name + "\n" + repositoryUrl);
            auto pending = std::make_shared<int>(manifest.pluginLists.size());
            auto allPlugins = std::make_shared<QList<CloudStream::PluginInfo>>();
            auto errors = std::make_shared<QStringList>();
            const auto manifestUrl = manifestReply->url();
            for (const auto &pluginList : manifest.pluginLists) {
                auto pluginRequest = cloudStreamRequest(manifestUrl.resolved(QUrl(pluginList)));
                auto *pluginReply = network.get(pluginRequest);
                connect(pluginReply, &QNetworkReply::finished, this,
                        [this, pluginReply, repositoryUrl, pending, allPlugins, errors] {
                    QList<CloudStream::PluginInfo> parsed;
                    QString listError;
                    if (pluginReply->error() == QNetworkReply::NoError &&
                        CloudStream::RepositoryManifestParser::parsePluginList(pluginReply->readAll(), &parsed, &listError)) {
                        allPlugins->append(parsed);
                    } else {
                        errors->append(pluginReply->error() == QNetworkReply::NoError ? listError : pluginReply->errorString());
                    }
                    pluginReply->deleteLater();
                    --*pending;
                    if (*pending != 0) return;
                    QList<CloudStream::PluginInfo> unique;
                    QStringList seen;
                    for (const auto &plugin : *allPlugins) {
                        const auto key = plugin.internalName + "\n" + plugin.url;
                        if (seen.contains(key)) continue;
                        seen << key;
                        unique << plugin;
                    }
                    populateExtensionCatalog(unique, repositoryUrl);
                    if (unique.isEmpty() && !errors->isEmpty()) {
                        status->setText("Plugin lists failed: " + errors->first());
                    } else if (!errors->isEmpty()) {
                        status->setText(QString::number(unique.size()) + " extension(s) available • some lists failed");
                    } else {
                        qInfo() << unique.size() << "extension(s) available";
                    }
                });
            }
            if (manifest.pluginLists.isEmpty()) {
                populateExtensionCatalog({}, repositoryUrl);
                status->setText("Repository contains no plugin lists");
            }
            manifestReply->deleteLater();
        });
    }

    void installExtension(const CloudStream::PluginInfo &plugin, const QString &repositoryUrl,
                          std::function<void(bool)> completion = {}) {
        const auto androidUrl = plugin.url;
        const auto androidHash = plugin.fileHash;
        const auto name = plugin.internalName;
        const auto jarUrl = plugin.jarUrl;
        const auto jarHash = plugin.jarHash;
        const auto displayName = plugin.name;
        const auto version = plugin.version;
        const auto language = plugin.language;
        const auto tvTypes = plugin.tvTypes;
        const auto iconUrl = plugin.iconUrl;
        const bool linuxCapable = !jarUrl.isEmpty();
        const auto url = linuxCapable ? jarUrl : androidUrl;
        const auto expected = linuxCapable ? jarHash : androidHash;
        if (url.isEmpty()) {
            status->setText("Extension has no download URL");
            if (completion) completion(false);
            return;
        }
        status->setText("Downloading " + name + "…");
        auto downloadRequest = cloudStreamRequest(QUrl(url));
        auto *reply = network.get(downloadRequest);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, expected, name, displayName, version, language, tvTypes, repositoryUrl, iconUrl, linuxCapable, completion] {
            if (reply->error() != QNetworkReply::NoError) {
                status->setText("Extension download failed");
                reply->deleteLater();
                if (completion) completion(false);
                return;
            }
            const auto bytes = reply->readAll();
            const auto actual = "sha256-" + QString(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
            if (!expected.isEmpty() && expected.compare(actual, Qt::CaseInsensitive) != 0) {
                status->setText("Checksum mismatch — extension rejected");
                reply->deleteLater();
                if (completion) completion(false);
                return;
            }
            auto safeName = name;
            safeName.replace(QRegularExpression("[^A-Za-z0-9_.-]"), "_");
            const auto repositoryId = QString::fromLatin1(QCryptographicHash::hash(repositoryUrl.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
            auto folder = CloudStream::XdgPaths::extensionsDir() + "/" + repositoryId + "/" + safeName + "/" + QString::number(std::max(0, version));
            QDir().mkpath(folder);
            const auto target = folder + "/" + safeName + (linuxCapable ? ".jar" : ".cs3");
            const auto temporary = target + ".part";
            QFile temp(temporary);
            if (!temp.open(QIODevice::WriteOnly) || temp.write(bytes) != bytes.size()) {
                status->setText("Could not save extension");
                temp.remove();
                reply->deleteLater();
                if (completion) completion(false);
                return;
            }
            temp.close();
            QFile::remove(target);
            if (!QFile::rename(temporary, target)) {
                temp.remove();
                status->setText("Could not activate downloaded extension");
                reply->deleteLater();
                if (completion) completion(false);
                return;
            }
            reply->deleteLater();

            auto installRecord = [this, name, displayName, repositoryUrl, iconUrl, target, version, language, tvTypes, actual, completion]
                    (const QString &platform, bool enabled) -> CloudStream::ExtensionRecord {
                for (const auto &previous : extensionRegistry.extensions()) {
                    if (previous.internalName == name && previous.repositoryUrl == repositoryUrl &&
                        previous.artifactPath != target && QFileInfo::exists(previous.artifactPath)) QFile::remove(previous.artifactPath);
                }
                CloudStream::ExtensionRecord record;
                record.internalName = name;
                record.displayName = displayName.isEmpty() ? name : displayName;
                record.iconUrl = iconUrl;
                record.repositoryUrl = repositoryUrl;
                record.artifactPath = target;
                record.sourceArtifactPath = platform == "android" ? target : QString();
                record.platform = platform;
                record.converterId = platform == "jvm" ? "native" : QString();
                record.version = version;
                record.language = language;
                record.tvTypes = tvTypes;
                record.sha256 = actual;
                record.enabled = enabled;
                extensionRegistry.upsertExtension(record);
                refreshInstalledExtensionsUi();
                if (!completion) loadExtensions();
                return record;
            };

            if (!linuxCapable) {
                const auto record = installRecord("android", false);
                if (completion) convertAndroidExtension(record, false, completion);
                else convertAndroidExtension(record, true);
                return;
            }

            const auto helper = CloudStream::ProviderHostCommand::discover();
            if (helper.isEmpty()) {
                installRecord("jvm", false);
                status->setText("JVM extension downloaded but provider host is missing");
                if (completion) completion(true);
                return;
            }
            auto *validation = new QProcess(this);
            helper.configure(validation, {"list", target, "auto"});
            CloudStream::ProcessCompletion::watch(validation, this,
                    [this, validation, installRecord, target, displayName, name, completion]
                    (int exitCode, QProcess::ExitStatus, bool) {
                const auto providers = QJsonDocument::fromJson(validation->readAllStandardOutput()).array();
                const auto valid = exitCode == 0 && !providers.isEmpty();
                const auto installedRecord = installRecord("jvm", valid);
                if (valid) {
                    stageValidatedProviders(installedRecord, providers);
                    if (!completion) settings.setValue("homeProviderKey", target + "\n" + providers.first().toObject().value("name").toString());
                    status->setText("Installed " + (displayName.isEmpty() ? name : displayName) + " • " +
                        QString::number(providers.size()) + " provider(s) loaded");
                    if (!completion) publishStagedProviders();
                } else {
                    status->setText("Extension downloaded but disabled: it registered no compatible provider");
                }
                validation->deleteLater();
                if (completion) completion(true);
            });
            validation->start();
            QTimer::singleShot(15000, validation, [validation] {
                if (validation->state() != QProcess::NotRunning) validation->kill();
            });
        });
    }

    void downloadSelectedExtension() {
        auto *item = extensions ? extensions->currentItem() : nullptr;
        if (!item || !item->data(Qt::UserRole + 11).isValid()) {
            status->setText("Select an extension first");
            return;
        }
        const auto index = item->data(Qt::UserRole + 11).toInt();
        if (index < 0 || index >= currentCatalogPlugins.size()) {
            status->setText("Extension catalog changed — refresh and try again");
            return;
        }
        installExtension(currentCatalogPlugins[index], currentCatalogRepositoryUrl);
    }

    void installAllExtensions(bool unattended = false,
                              std::function<void(int, int)> completion = {}) {
        if (bulkExtensionInstallActive) return;
        const auto pending = CloudStream::ExtensionInstallBatch::pending(
            currentCatalogPlugins, extensionRegistry.extensions(), currentCatalogRepositoryUrl);
        if (pending.isEmpty()) {
            status->setText("Every installable extension in this repository is current");
            refreshInstallAllButton();
            if (completion) completion(0, 0);
            return;
        }
        if (!unattended && QMessageBox::question(this, "Install all extensions",
                "Install or update " + QString::number(pending.size()) +
                " extension(s) from the selected repository?\n\n"
                "Each package will be checksum-verified, converted when required, "
                "and validated before the next package starts.") != QMessageBox::Yes) return;

        bulkExtensionInstallActive = true;
        refreshInstallAllButton();
        auto queue = std::make_shared<QList<CloudStream::PluginInfo>>(pending);
        auto index = std::make_shared<int>(0);
        auto installed = std::make_shared<int>(0);
        auto failed = std::make_shared<int>(0);
        const auto repositoryUrl = currentCatalogRepositoryUrl;
        const auto catalog = currentCatalogPlugins;
        auto next = std::make_shared<std::function<void()>>();
        *next = [this, queue, index, installed, failed, repositoryUrl, catalog,
                 next, unattended, completion] {
            if (*index >= queue->size()) {
                bulkExtensionInstallActive = false;
                refreshInstalledExtensionsUi();
                publishStagedProviders();
                refreshProviderChoices(true);
                if (currentCatalogRepositoryUrl == repositoryUrl) {
                    populateExtensionCatalog(catalog, repositoryUrl);
                } else {
                    refreshInstallAllButton();
                }
                status->setText("Install all finished • " + QString::number(*installed) +
                    " installed • " + QString::number(*failed) + " failed");
                if (!unattended) {
                    QMessageBox::information(this, "Install all finished",
                        QString::number(*installed) + " extension(s) installed or updated.\n" +
                        QString::number(*failed) + " extension(s) failed; failed packages were not enabled.");
                }
                if (completion) completion(*installed, *failed);
                QTimer::singleShot(0, this, [next] { *next = {}; });
                return;
            }
            const auto plugin = queue->at(*index);
            ++*index;
            const auto label = plugin.name.isEmpty() ? plugin.internalName : plugin.name;
            status->setText("Installing " + QString::number(*index) + " of " +
                QString::number(queue->size()) + " • " + label);
            installExtension(plugin, repositoryUrl,
                [this, installed, failed, next](bool success) {
                    if (success) ++*installed;
                    else ++*failed;
                    QTimer::singleShot(0, this, [next] { if (*next) (*next)(); });
                });
        };
        (*next)();
    }

    QString resolveRepositoryCode(const QString &value) const {
        return CloudStream::RepositoryUrlResolver::resolveShortForm(value);
    }

    void importRepositoryIndexDocument(const QUrl &indexUrl, const QByteArray &document) {
        QList<CloudStream::RepositoryIndexEntry> entries;
        QString parseError;
        if (!CloudStream::RepositoryManifestParser::parseRepositoryIndex(document, &entries, &parseError)) {
            status->setText("Repository index rejected");
            QMessageBox::warning(this, "Invalid repository index", parseError);
            return;
        }
        status->setText("Validating " + QString::number(entries.size()) + " repositories…");
        auto pending = std::make_shared<int>(entries.size());
        auto imported = std::make_shared<int>(0);
        auto failed = std::make_shared<QStringList>();
        for (const auto &entry : entries) {
            auto request = cloudStreamRequest(QUrl(entry.url));
            auto *reply = network.get(request);
            connect(reply, &QNetworkReply::finished, this,
                    [this, reply, entry, indexUrl, pending, imported, failed, total = entries.size()] {
                CloudStream::RepositoryManifest manifest;
                QString childError;
                if (reply->error() == QNetworkReply::NoError &&
                    CloudStream::RepositoryManifestParser::parseManifest(reply->readAll(), &manifest, &childError)) {
                    const auto finalUrl = reply->url().toString();
                    if (extensionRegistry.addRepository({manifest.name.isEmpty() ? entry.name : manifest.name, finalUrl})) {
                        ++*imported;
                    } else {
                        failed->append(entry.url + ": could not save");
                    }
                } else {
                    failed->append(entry.url + ": " +
                        (reply->error() == QNetworkReply::NoError ? childError : reply->errorString()));
                }
                reply->deleteLater();
                --*pending;
                if (*pending != 0) return;
                refreshRepositoryList();
                status->setText("Imported " + QString::number(*imported) + " of " + QString::number(total) +
                    " repositories from " + indexUrl.host() +
                    (failed->isEmpty() ? QString() : " • " + QString::number(failed->size()) + " failed"));
                if (!failed->isEmpty()) {
                    QMessageBox::information(this, "Repository index imported",
                        QString::number(*imported) + " repositories were added or refreshed.\n\n" +
                        QString::number(failed->size()) + " entries failed validation:\n" + failed->join("\n"));
                }
            });
        }
    }

    void importRepositoryIndexFromUrl(const QString &url) {
        auto request = cloudStreamRequest(QUrl(url));
        status->setText("Loading repository index…");
        auto *reply = network.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            if (reply->error() != QNetworkReply::NoError) {
                status->setText("Repository index unavailable");
                QMessageBox::warning(this, "Repository index unavailable", reply->errorString());
                reply->deleteLater();
                return;
            }
            const auto finalUrl = reply->url();
            const auto body = reply->readAll();
            reply->deleteLater();
            CloudStream::RepositoryManifest manifest;
            QString manifestError;
            if (CloudStream::RepositoryManifestParser::parseManifest(body, &manifest, &manifestError)) {
                extensionRegistry.addRepository({manifest.name, finalUrl.toString()});
                settings.setValue("selectedRepositoryUrl", finalUrl.toString());
                refreshRepositoryList();
                loadExtensions();
                status->setText("Repository added: " + manifest.name);
                return;
            }
            importRepositoryIndexDocument(finalUrl, body);
        });
    }

    void addRepository() {
        bool ok = false;
        const auto entered = QInputDialog::getText(this, "Add extension repository", "Repository URL or short code:", QLineEdit::Normal, "https://", &ok).trimmed();
        if (!ok || entered.isEmpty()) return;
        const auto resolved = resolveRepositoryCode(entered);
        if (!CloudStream::RepositoryUrlResolver::isSupportedInput(entered)) { QMessageBox::warning(this, "Invalid repository", "Use an HTTPS URL, cloudstreamrepo:// link, cs.repo link, or supported short code."); return; }
        status->setText("Checking repository…");
        auto request = cloudStreamRequest(QUrl(repositoryManifestUrl(resolved)));
        auto *reply = network.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            const auto finalUrl = reply->url().toString();
            if (reply->error() != QNetworkReply::NoError || finalUrl.isEmpty()) {
                status->setText("Repository could not be reached");
                QMessageBox::warning(this, "Repository unavailable", reply->errorString());
            } else {
                const auto body = reply->readAll();
                CloudStream::RepositoryManifest manifest;
                QString parseError;
                if (!CloudStream::RepositoryManifestParser::parseManifest(body, &manifest, &parseError)) {
                    QList<CloudStream::RepositoryIndexEntry> indexEntries;
                    QString indexError;
                    if (CloudStream::RepositoryManifestParser::parseRepositoryIndex(body, &indexEntries, &indexError)) {
                        const auto indexUrl = reply->url();
                        reply->deleteLater();
                        importRepositoryIndexDocument(indexUrl, body);
                        return;
                    }
                    status->setText("Repository document rejected");
                    QMessageBox::warning(this, "Invalid repository", parseError + "\n" + indexError);
                    reply->deleteLater();
                    return;
                }
                extensionRegistry.addRepository({manifest.name, finalUrl});
                settings.setValue("selectedRepositoryUrl", finalUrl);
                refreshRepositoryList();
                loadExtensions();
                status->setText("Repository added: " + manifest.name);
            }
            reply->deleteLater();
        });
    }

    QString saveProviderHistory(const QJsonObject &details, const QString &jar, const QString &provider,
                                const QString &sourceUrl, const QString &playbackData,
                                const QString &episodeName) {
        const auto id = CloudStream::WatchHistoryStore::idFor(provider, sourceUrl);
        CloudStream::WatchEntry entry;
        for (const auto &existing : history.entries()) {
            if (existing.id == id) {
                entry = existing;
                break;
            }
        }
        if (entry.playbackData != playbackData) {
            entry.positionSeconds = 0.0;
            entry.durationSeconds = 0.0;
        }
        entry.id = id;
        entry.name = details.value("name").toString("Untitled");
        entry.sourceUrl = sourceUrl;
        entry.provider = provider;
        entry.jarPath = jar;
        entry.posterUrl = details.value("posterUrl").toString();
        entry.playbackData = playbackData;
        entry.episodeName = episodeName;
        entry.state = "Watching";
        entry.updatedAt = 0;
        if (history.upsert(entry)) {
            refreshContinueWatching();
            if (libraryLoaded && libraryRefresh) libraryRefresh();
        }
        return id;
    }

    void queueSourceForDownload(const CloudStream::PlaybackSource &source,
                                const QString &displayTitle, const QString &artifact,
                                const QString &provider, const QString &playbackData) {
        const auto downloadPath = settings.value(
            "downloadFolder", QDir::homePath() + "/Downloads").toString();
        QString error;
        CloudStream::DownloadOrigin origin;
        origin.artifactPath = artifact;
        origin.provider = provider;
        origin.playbackData = playbackData;
        const auto id = downloadManager.enqueue(
            displayTitle.isEmpty() ? QStringLiteral("CloudStream download") : displayTitle,
            source, downloadPath, &error, origin);
        if (id.isEmpty()) {
            QMessageBox::warning(this, "Download unavailable", error);
            return;
        }
        if (downloadsRefresh) downloadsRefresh();
        downloadManager.start();
        status->setText("Queued " + displayTitle + " for download");
    }

    void resolveAndPlay(const QString &jar, const QString &provider, const QString &data,
                        const QString &historyId = {}, const QString &displayTitle = {},
                        bool chooseSource = false, bool downloadSource = false,
                        QObject *requestContext = nullptr) {
        const auto playbackGeneration = playbackRequestGeneration.begin();
        if (activePlaybackResolutionProcess &&
            activePlaybackResolutionProcess->state() != QProcess::NotRunning) {
            activePlaybackResolutionProcess->kill();
        }
        if (data.isEmpty()) { status->setText("This item has no playback data"); return; }
        const auto helper = CloudStream::ProviderHostCommand::discover();
        if (helper.isEmpty()) {
            status->setText("Provider host is not installed");
            return;
        }
        auto *process = new QProcess(this);
        activePlaybackResolutionProcess = process;
        const QPointer<QObject> safeRequestContext(requestContext ? requestContext : this);
        if (requestContext) {
            connect(requestContext, &QObject::destroyed, process, [process] {
                if (process->state() != QProcess::NotRunning) process->kill();
            });
        }
        helper.configure(process, {"sources", jar, "auto", provider, data});
        status->setText("Finding playable hosters and qualities…");
        CloudStream::ProcessCompletion::watch(process, this,
                [this, process, historyId, displayTitle, jar, provider, data, chooseSource,
                 downloadSource, playbackGeneration, safeRequestContext]
                (int exitCode, QProcess::ExitStatus, bool startFailure) {
            if (activePlaybackResolutionProcess == process) {
                activePlaybackResolutionProcess = nullptr;
            }
            if (!safeRequestContext ||
                !playbackRequestGeneration.isCurrent(playbackGeneration)) {
                process->deleteLater();
                return;
            }
            auto discovery = CloudStream::SourceCatalog::parse(process->readAllStandardOutput());
            if (exitCode != 0 || discovery.sources.isEmpty()) {
                const auto diagnostic = startFailure ? process->errorString()
                    : QString::fromUtf8(process->readAllStandardError()).trimmed();
                status->setText(diagnostic.isEmpty()
                    ? "Provider returned no playable HLS, DASH, or video sources"
                    : "Source discovery failed: " + diagnostic.section('\n', 0, 0));
                process->deleteLater();
                return;
            }
            double resumePosition = 0.0;
            if (!historyId.isEmpty()) {
                for (const auto &entry : history.entries()) {
                    if (entry.id == historyId && entry.state != "Completed") {
                        resumePosition = entry.positionSeconds;
                        break;
                    }
                }
            }
            if (!chooseSource && !downloadSource) {
                const auto qualitySetting = settings.value(
                    "player/quality", "Automatic (best)").toString();
                const auto desiredQuality = qualitySetting.section('p', 0, 0).toInt();
                if (desiredQuality > 0) {
                    const auto preferred = std::find_if(
                        discovery.sources.cbegin(), discovery.sources.cend(),
                        [desiredQuality](const auto &source) {
                            return source.quality == desiredQuality;
                        });
                    if (preferred != discovery.sources.cend()) {
                        const auto preferredIndex = int(std::distance(
                            discovery.sources.cbegin(), preferred));
                        if (preferredIndex > 0) {
                            discovery.sources.prepend(discovery.sources.takeAt(preferredIndex));
                        }
                    }
                }
            }
            int selectedSubtitleOverride = -2;
            if (chooseSource || downloadSource) {
                if (downloadSource && std::none_of(discovery.sources.cbegin(), discovery.sources.cend(),
                        [](const auto &source) { return CloudStream::DownloadManager::isDownloadable(source); })) {
                    status->setText("This provider returned playback-only stream protocols");
                    QMessageBox::information(this, "Offline download unavailable",
                        "None of this title's sources can be saved safely by the installed download engines.");
                    process->deleteLater();
                    return;
                }
                QDialog sourceDialog(this);
                sourceDialog.setObjectName("sourcePickerDialog");
                sourceDialog.setWindowTitle("Choose source");
                sourceDialog.resize(980, 620);
                sourceDialog.setMinimumSize(760, 520);
                auto *sourceLayout = new QVBoxLayout(&sourceDialog);
                sourceLayout->setContentsMargins(22, 20, 22, 20);
                sourceLayout->setSpacing(12);
                sourceLayout->addWidget(title("Choose a video source", 23));
                auto *sourceIntro = new QLabel(QString::number(discovery.sources.size()) +
                    " playable source(s) found. If your choice fails, the player retains the other listed "
                    "sources as fallbacks.");
                sourceIntro->setObjectName("muted");
                sourceIntro->setWordWrap(true);
                sourceLayout->addWidget(sourceIntro);
                auto *chooser = new QSplitter(Qt::Horizontal);
                chooser->setObjectName("sourceSubtitleChooser");
                auto *sourcePane = new QWidget;
                sourcePane->setObjectName("sourcePane");
                auto *sourcePaneLayout = new QVBoxLayout(sourcePane);
                sourcePaneLayout->setContentsMargins(0, 0, 10, 0);
                sourcePaneLayout->setSpacing(9);
                sourcePaneLayout->addWidget(title("Sources", 17));
                auto *sourceList = new QListWidget;
                sourceList->setObjectName("sourceList");
                sourceList->setIconSize(QSize(32, 32));
                for (int index = 0; index < discovery.sources.size(); ++index) {
                    const auto &candidate = discovery.sources[index];
                    auto heading = candidate.name.trimmed();
                    if (heading.isEmpty()) {
                        heading = candidate.hosterName();
                        if (candidate.quality > 0) heading += "  •  " + QString::number(candidate.quality) + "p";
                    }
                    const auto type = candidate.type.toUpper();
                    const auto transport = type == "M3U8" ? QStringLiteral("Adaptive HLS • offline remux available")
                        : type == "DASH" ? QStringLiteral("Adaptive DASH • offline remux available")
                        : QStringLiteral("Direct video • offline download available");
                    const auto rowText = heading + "\n" + candidate.hosterName() + "  •  " + transport;
                    auto *item = new QListWidgetItem(QIcon(":/icons/source-selector.svg"), rowText);
                    item->setData(Qt::UserRole, index);
                    item->setData(Qt::UserRole + 1, rowText);
                    sourceList->addItem(item);
                }
                int initialSource = 0;
                if (downloadSource) {
                    for (int index = 0; index < discovery.sources.size(); ++index) {
                        if (CloudStream::DownloadManager::isDownloadable(discovery.sources[index])) {
                            initialSource = index;
                            break;
                        }
                    }
                }
                sourceList->setCurrentRow(initialSource);
                sourcePaneLayout->addWidget(sourceList, 1);
                auto *sourceDetails = new QLabel;
                sourceDetails->setObjectName("sourceMetadata");
                sourceDetails->setWordWrap(true);
                sourcePaneLayout->addWidget(sourceDetails);
                chooser->addWidget(sourcePane);

                auto *subtitlePane = new QWidget;
                subtitlePane->setObjectName("subtitlePane");
                auto *subtitlePaneLayout = new QVBoxLayout(subtitlePane);
                subtitlePaneLayout->setContentsMargins(10, 0, 0, 0);
                subtitlePaneLayout->setSpacing(9);
                auto *subtitleHeading = new QHBoxLayout;
                subtitleHeading->addWidget(title("Subtitles", 17));
                subtitleHeading->addStretch();
                auto *subtitleMode = new QLabel("Auto");
                subtitleMode->setObjectName("muted");
                subtitleHeading->addWidget(subtitleMode);
                subtitlePaneLayout->addLayout(subtitleHeading);
                auto *subtitleList = new QListWidget;
                subtitleList->setObjectName("subtitleList");
                auto *noSubtitles = new QListWidgetItem(
                    QIcon(":/icons/subtitles.svg"), "No subtitles");
                noSubtitles->setData(Qt::UserRole, -1);
                noSubtitles->setData(Qt::UserRole + 1, "No subtitles");
                subtitleList->addItem(noSubtitles);
                for (int index = 0; index < discovery.subtitles.size(); ++index) {
                    const auto &subtitle = discovery.subtitles[index];
                    const auto label = subtitle.language.trimmed().isEmpty()
                        ? QStringLiteral("Subtitle track") : subtitle.language;
                    auto *item = new QListWidgetItem(QIcon(":/icons/subtitles.svg"), label);
                    item->setData(Qt::UserRole, index);
                    item->setData(Qt::UserRole + 1, label);
                    subtitleList->addItem(item);
                }
                const bool preferFirstSubtitle = settings.value(
                    "player/subtitles", "Off").toString() == "First available";
                subtitleList->setCurrentRow(
                    preferFirstSubtitle && subtitleList->count() > 1 ? 1 : 0);
                subtitlePaneLayout->addWidget(subtitleList, 1);
                auto *subtitleActions = new QVBoxLayout;
                auto *loadSubtitle = button("Load from file");
                loadSubtitle->setIcon(QIcon(":/icons/extension.svg"));
                auto *onlineSubtitle = button("Search online");
                onlineSubtitle->setIcon(QIcon(":/icons/search.svg"));
                onlineSubtitle->setEnabled(false);
                onlineSubtitle->setToolTip(
                    "Online subtitle search is not implemented in the Linux client yet");
                subtitleActions->addWidget(loadSubtitle);
                subtitleActions->addWidget(onlineSubtitle);
                subtitlePaneLayout->addLayout(subtitleActions);
                chooser->addWidget(subtitlePane);
                chooser->setStretchFactor(0, 3);
                chooser->setStretchFactor(1, 2);
                sourceLayout->addWidget(chooser, 1);
                auto *buttons = new QDialogButtonBox;
                auto *downloadButton = buttons->addButton("Download selected source", QDialogButtonBox::ActionRole);
                downloadButton->setIcon(QIcon(downloadSource
                    ? ":/icons/download-dark.svg" : ":/icons/download.svg"));
                auto *playButton = buttons->addButton("Apply and play", QDialogButtonBox::AcceptRole);
                playButton->setIcon(QIcon(downloadSource
                    ? ":/icons/play.svg" : ":/icons/play-dark.svg"));
                buttons->addButton(QDialogButtonBox::Cancel);
                (downloadSource ? downloadButton : playButton)->setProperty("primary", true);
                const auto updateSourceDetails = [sourceList, sourceDetails, downloadButton, &discovery] {
                    const auto index = sourceList->currentItem()
                        ? sourceList->currentItem()->data(Qt::UserRole).toInt() : -1;
                    if (index < 0 || index >= discovery.sources.size()) {
                        sourceDetails->clear();
                        downloadButton->setEnabled(false);
                        return;
                    }
                    const auto &candidate = discovery.sources[index];
                    QStringList identity{"Hoster: " + candidate.hosterName()};
                    if (candidate.quality > 0) identity << "Quality: " + QString::number(candidate.quality) + "p";
                    if (!candidate.type.isEmpty()) identity << "Protocol: " + candidate.type.toUpper();
                    QStringList requestFacts;
                    if (!candidate.referer.isEmpty()) requestFacts << "Referer preserved";
                    if (!candidate.headers.isEmpty()) requestFacts << QString::number(candidate.headers.size()) + " request header(s)";
                    requestFacts << (discovery.subtitles.isEmpty()
                        ? QStringLiteral("No subtitles from provider")
                        : QString::number(discovery.subtitles.size()) + " subtitle track(s)");
                    const bool direct = CloudStream::DownloadManager::isDirectDownload(candidate);
                    const bool adaptive = CloudStream::DownloadManager::isAdaptiveDownload(candidate);
                    const bool downloadable = direct || adaptive;
                    downloadButton->setEnabled(downloadable);
                    downloadButton->setToolTip(direct
                        ? "Download this direct video with resumable HTTP Range support"
                        : adaptive ? "Save this adaptive stream as a local Matroska video"
                                   : "This source protocol is playback-only");
                    sourceDetails->setText(identity.join("  •  ") + "\n" +
                        requestFacts.join("  •  ") + "\n" +
                        (direct ? "Offline download: available with resume support"
                                : adaptive ? "Offline download: available through adaptive-stream remux"
                                           : "Offline download: unavailable for this protocol"));
                };
                const auto updateSourceSelection = [sourceList] {
                    for (int row = 0; row < sourceList->count(); ++row) {
                        auto *item = sourceList->item(row);
                        const bool selected = item == sourceList->currentItem();
                        item->setIcon(QIcon(selected ? ":/icons/check.svg" : ":/icons/source-selector.svg"));
                        item->setText((selected ? "Selected  •  " : QString()) +
                                      item->data(Qt::UserRole + 1).toString());
                    }
                };
                const auto updateSubtitleSelection = [subtitleList] {
                    for (int row = 0; row < subtitleList->count(); ++row) {
                        auto *item = subtitleList->item(row);
                        const bool selected = item == subtitleList->currentItem();
                        item->setIcon(QIcon(selected ? ":/icons/check.svg" : ":/icons/subtitles.svg"));
                        item->setText((selected ? "Selected  •  " : QString()) +
                                      item->data(Qt::UserRole + 1).toString());
                    }
                };
                connect(sourceList, &QListWidget::currentItemChanged, &sourceDialog,
                        [updateSourceDetails, updateSourceSelection](QListWidgetItem *, QListWidgetItem *) {
                    updateSourceSelection();
                    updateSourceDetails();
                });
                updateSourceSelection();
                updateSourceDetails();
                connect(subtitleList, &QListWidget::currentItemChanged, &sourceDialog,
                        [updateSubtitleSelection](QListWidgetItem *, QListWidgetItem *) {
                    updateSubtitleSelection();
                });
                updateSubtitleSelection();
                connect(loadSubtitle, &QPushButton::clicked, &sourceDialog,
                        [this, &sourceDialog, subtitleList, &discovery, updateSubtitleSelection] {
                    showInAppFolderDialog(QDir::homePath(), "Load subtitle file", false,
                        [&sourceDialog, subtitleList, &discovery, updateSubtitleSelection](const QString &file) {
                    CloudStream::PlaybackSubtitle subtitle;
                    subtitle.language = QFileInfo(file).completeBaseName();
                    subtitle.url = file;
                    discovery.subtitles.append(subtitle);
                    auto *item = new QListWidgetItem(
                        QIcon(":/icons/subtitles.svg"), subtitle.language);
                    item->setData(Qt::UserRole, discovery.subtitles.size() - 1);
                    item->setData(Qt::UserRole + 1, subtitle.language);
                    subtitleList->addItem(item);
                    subtitleList->setCurrentItem(item);
                    updateSubtitleSelection();
                });
                });
                connect(playButton, &QPushButton::clicked, &sourceDialog, &QDialog::accept);
                connect(downloadButton, &QPushButton::clicked, &sourceDialog,
                        [&sourceDialog] { sourceDialog.done(2); });
                connect(buttons, &QDialogButtonBox::rejected, &sourceDialog, &QDialog::reject);
                connect(sourceList, &QListWidget::itemActivated, &sourceDialog,
                        [&sourceDialog, downloadSource](QListWidgetItem *) {
                    sourceDialog.done(downloadSource ? 2 : QDialog::Accepted);
                });
                sourceLayout->addWidget(buttons);
                CloudStream::SmoothScrollController::attachRecursively(&sourceDialog);
                const auto previewPath = sourcePreviewOutputPath;
                if (!previewPath.isEmpty()) {
                    QTimer::singleShot(400, &sourceDialog, [&sourceDialog, previewPath] {
                        sourceDialog.grab().save(previewPath);
                        sourceDialog.reject();
                    });
                }
                const auto sourceResult = sourceDialog.exec();
                if (!previewPath.isEmpty()) sourcePreviewOutputPath.clear();
                if (!safeRequestContext ||
                    !playbackRequestGeneration.isCurrent(playbackGeneration)) {
                    process->deleteLater();
                    return;
                }
                if (sourceResult != QDialog::Accepted) {
                    if (sourceResult == 2) {
                        const auto index = sourceList->currentItem()
                            ? sourceList->currentItem()->data(Qt::UserRole).toInt() : -1;
                        if (index >= 0 && index < discovery.sources.size()) {
                            queueSourceForDownload(discovery.sources[index], displayTitle,
                                                   jar, provider, data);
                        }
                    }
                    process->deleteLater();
                    return;
                }
                const auto index = sourceList->currentItem()
                    ? sourceList->currentItem()->data(Qt::UserRole).toInt() : 0;
                if (index > 0 && index < discovery.sources.size()) {
                    discovery.sources.prepend(discovery.sources.takeAt(index));
                }
                selectedSubtitleOverride = subtitleList->currentItem()
                    ? subtitleList->currentItem()->data(Qt::UserRole).toInt() : -1;
                if (selectedSubtitleOverride >= 0 &&
                    selectedSubtitleOverride < discovery.subtitles.size()) {
                    discovery.subtitles.prepend(
                        discovery.subtitles.takeAt(selectedSubtitleOverride));
                }
            }
            if (integratedPlayer) integratedPlayer->close();
            CloudStream::PlayerPreferences playerPreferences;
            playerPreferences.seekSeconds = settings.value("player/seekSeconds", 10).toInt();
            const bool muteNsfw = settings.value(
                "providers/muteNsfwByDefault", true).toBool() && isNsfwProvider(jar, provider);
            playerPreferences.initialVolume = muteNsfw
                ? 0 : settings.value("player/volume", 80).toInt();
            playerPreferences.automaticFallback = settings.value(
                "player/automaticFallback", true).toBool();
            playerPreferences.showInformation = settings.value(
                "player/showInformation", true).toBool();
            playerPreferences.selectFirstSubtitle = selectedSubtitleOverride == -2
                ? settings.value("player/subtitles", "Off").toString() == "First available"
                : selectedSubtitleOverride >= 0;
            auto *window = new CloudStream::IntegratedPlayerWindow(
                discovery, displayTitle.isEmpty() ? provider : displayTitle,
                resumePosition, this, playerPreferences);
            if (settings.value("interface/windowMode", "Separate windows").toString() ==
                    "Single-window navigation" && pages) {
                window->setWindowFlags(Qt::Widget);
                window->setParent(pages);
                window->setGeometry(pages->rect());
            }
            integratedPlayer = window;
            connect(window, &CloudStream::IntegratedPlayerWindow::progressUpdated, this,
                    [this, historyId](double position, double duration) {
                if (!historyId.isEmpty() && duration > 0.0) history.updateProgress(historyId, position, duration);
            });
            connect(window, &QObject::destroyed, this, [this] {
                refreshContinueWatching();
                if (libraryLoaded && libraryRefresh) libraryRefresh();
            });
            QSet<QString> hosters;
            for (const auto &source : discovery.sources) hosters.insert(source.hosterName());
            status->setText("Found " + QString::number(discovery.sources.size()) + " playable source(s) from " +
                            QString::number(hosters.size()) + " hoster(s)");
            window->show();
            window->raise();
            process->deleteLater();
        });
        process->start();
        QTimer::singleShot(30000, process, [process] { if (process->state() != QProcess::NotRunning) process->kill(); });
    }

    void openDetails(const QString &jar, const QString &provider, const QString &url,
                     bool autoPlay = false) {
        const auto helper = CloudStream::ProviderHostCommand::discover();
        if (helper.isEmpty()) {
            QMessageBox::warning(this, "Provider host missing", "The CloudStream provider host is not installed.");
            return;
        }
        const bool singleWindow = settings.value(
            "interface/windowMode", "Separate windows").toString() == "Single-window navigation";
        auto *dialog = new QDialog(singleWindow && pages ? static_cast<QWidget *>(pages) : this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setObjectName("detailsDialog");
        dialog->setWindowTitle("Loading details…");
        dialog->resize(1120, 800);
        dialog->setMinimumSize(760, 600);
        if (singleWindow && pages) {
            dialog->setWindowFlags(Qt::Widget);
            dialog->setGeometry(pages->rect());
            activeDetailsDialog = dialog;
            connect(dialog, &QObject::destroyed, this, [this] {
                activeDetailsDialog = nullptr;
            });
        }
        auto *root = new QVBoxLayout(dialog);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        auto *loading = new QLabel("Loading provider details…");
        loading->setObjectName("detailsLoading");
        loading->setAlignment(Qt::AlignCenter);
        root->addWidget(loading, 1);
        dialog->show();

        auto *process = new QProcess(dialog);
        helper.configure(process, {"load", jar, "auto", provider, url});
        const QPointer<QDialog> safeDialog(dialog);
        CloudStream::ProcessCompletion::watch(process, this,
                [this, process, safeDialog, root, loading, jar, provider, url, autoPlay]
                (int exitCode, QProcess::ExitStatus, bool startFailure) {
            if (!safeDialog) return;
            const auto details = QJsonDocument::fromJson(process->readAllStandardOutput()).object();
            if (exitCode != 0 || details.isEmpty()) {
                loading->setText("Could not load details.\n" +
                    (startFailure ? process->errorString()
                                  : QString::fromUtf8(process->readAllStandardError()).trimmed()));
                process->deleteLater();
                return;
            }
            root->removeWidget(loading);
            loading->deleteLater();
            safeDialog->setWindowTitle(details.value("name").toString());

            auto *scroll = new QScrollArea;
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            auto *content = new QWidget;
            content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            auto *contentLayout = new QVBoxLayout(content);
            contentLayout->setContentsMargins(22, 18, 22, 28);
            contentLayout->setSpacing(18);

            auto *hero = new HomeHeroBanner;
            hero->setObjectName("detailsHero");
            hero->setMinimumHeight(320);
            hero->setMaximumHeight(380);
            auto *heroLayout = new QHBoxLayout(hero);
            heroLayout->setContentsMargins(28, 24, 28, 26);
            heroLayout->setSpacing(22);
            auto *metadata = new QVBoxLayout;
            metadata->setSpacing(9);
            metadata->addStretch();
            auto *detailsTitle = title(details.value("name").toString(), 31);
            detailsTitle->setObjectName("detailsTitle");
            detailsTitle->setWordWrap(true);
            detailsTitle->setMaximumWidth(670);
            metadata->addWidget(detailsTitle);
            auto *factsLabel = new QLabel(CloudStream::DetailsPresentation::facts(details, provider).join("  •  "));
            factsLabel->setObjectName("detailsFacts");
            factsLabel->setWordWrap(true);
            metadata->addWidget(factsLabel);
            QStringList tags;
            for (const auto &tag : details.value("tags").toArray()) {
                if (!tag.toString().trimmed().isEmpty()) tags << tag.toString().trimmed();
            }
            if (!tags.isEmpty()) {
                auto *tagsLabel = new QLabel(tags.join("  •  "));
                tagsLabel->setObjectName("detailsTags");
                tagsLabel->setWordWrap(true);
                tagsLabel->setMaximumWidth(720);
                metadata->addWidget(tagsLabel);
            }
            if (details.value("comingSoon").toBool()) {
                auto *comingSoon = new QLabel("Coming soon");
                comingSoon->setObjectName("comingSoonBadge");
                comingSoon->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
                metadata->addWidget(comingSoon);
            }

            const auto episodeCatalog = CloudStream::EpisodeCatalog::fromJson(details.value("episodes").toArray());
            auto *episodes = new QListWidget;
            episodes->setObjectName("episodeList");
            episodes->setIconSize(CloudStream::ArtworkSizing::backdropSize(116));
            episodes->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            episodes->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            episodes->setTextElideMode(Qt::ElideRight);
            episodes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            const auto selectedPlayback = [details, episodes, episodeCatalog] {
                return episodeCatalog.isEmpty() ? details.value("data").toString()
                    : (episodes->currentItem() ? episodes->currentItem()->data(Qt::UserRole).toString() : QString());
            };
            const auto selectedEpisodeName = [episodes, episodeCatalog] {
                return episodeCatalog.isEmpty() || !episodes->currentItem()
                    ? QString() : episodes->currentItem()->data(Qt::UserRole + 1).toString();
            };
            const auto playSelection = [this, details, selectedPlayback, selectedEpisodeName,
                                         episodeCatalog, jar, provider, url,
                                         safeDialog](bool chooseSource) {
                const auto data = selectedPlayback();
                if (data.isEmpty()) {
                    status->setText(episodeCatalog.isEmpty()
                        ? "This title has no playback data" : "Choose an episode to play");
                    return;
                }
                const auto episodeName = selectedEpisodeName();
                const auto id = saveProviderHistory(details, jar, provider, url, data, episodeName);
                const auto playerTitle = episodeName.isEmpty() ? details.value("name").toString()
                    : details.value("name").toString() + " • " + episodeName;
                resolveAndPlay(jar, provider, data, id, playerTitle, chooseSource, false,
                               safeDialog);
            };
            const auto downloadSelection = [this, details, selectedPlayback, selectedEpisodeName,
                                             episodeCatalog, jar, provider, safeDialog] {
                const auto data = selectedPlayback();
                if (data.isEmpty()) {
                    status->setText(episodeCatalog.isEmpty()
                        ? "This title has no download data" : "Choose an episode to download");
                    return;
                }
                const auto episodeName = selectedEpisodeName();
                const auto downloadTitle = episodeName.isEmpty() ? details.value("name").toString()
                    : details.value("name").toString() + " • " + episodeName;
                resolveAndPlay(jar, provider, data, {}, downloadTitle, true, true,
                               safeDialog);
            };

            auto *heroActions = new QHBoxLayout;
            heroActions->setSpacing(9);
            auto *play = button(episodeCatalog.isEmpty() ? "Play" : "Play episode", true);
            play->setIcon(QIcon(":/icons/play-dark.svg"));
            auto *chooseSource = button("Sources");
            chooseSource->setIcon(QIcon(":/icons/source-selector.svg"));
            auto *download = button("Download");
            download->setIcon(QIcon(":/icons/download.svg"));
            const auto historyId = CloudStream::WatchHistoryStore::idFor(provider, url);
            bool saved = false;
            for (const auto &entry : history.entries()) {
                if (entry.id == historyId) { saved = true; break; }
            }
            auto *save = button(saved ? "In library" : "Add to library");
            save->setIcon(QIcon(saved ? ":/icons/bookmark-filled.svg" : ":/icons/bookmark-outline.svg"));
            auto *sourcePage = button("Provider page");
            sourcePage->setIcon(QIcon(":/icons/open-in-new.svg"));
            heroActions->addWidget(play);
            heroActions->addWidget(chooseSource);
            heroActions->addWidget(download);
            heroActions->addWidget(save);
            heroActions->addWidget(sourcePage);
            heroActions->addStretch();
            metadata->addLayout(heroActions);
            heroLayout->addLayout(metadata, 1);

            const auto posterUrl = details.value("posterUrl").toString();
            const auto backdropUrl = CloudStream::DetailsPresentation::backdropUrl(details);
            auto *poster = new QLabel;
            poster->setObjectName("detailsPoster");
            poster->setFixedSize(CloudStream::ArtworkSizing::posterSize(150));
            poster->setAlignment(Qt::AlignCenter);
            poster->setText("No poster");
            poster->setVisible(!posterUrl.isEmpty() && posterUrl != backdropUrl);
            heroLayout->addWidget(poster, 0, Qt::AlignBottom);
            contentLayout->addWidget(hero);

            auto *aboutTitle = title("About", 19);
            contentLayout->addWidget(aboutTitle);
            auto *plot = new QLabel(details.value("plot").toString("No description is available."));
            plot->setObjectName("detailsPlot");
            plot->setWordWrap(true);
            plot->setTextInteractionFlags(Qt::TextSelectableByMouse);
            plot->setMaximumWidth(860);
            plot->setMaximumHeight(84);
            contentLayout->addWidget(plot);
            if (plot->sizeHint().height() > 84) {
                auto *morePlot = button("More");
                morePlot->setProperty("chip", true);
                morePlot->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
                connect(morePlot, &QPushButton::clicked, this, [plot, morePlot] {
                    const bool expanded = plot->maximumHeight() > 1000;
                    plot->setMaximumHeight(expanded ? 84 : QWIDGETSIZE_MAX);
                    morePlot->setText(expanded ? "More" : "Less");
                });
                contentLayout->addWidget(morePlot);
            }

            if (!episodeCatalog.isEmpty()) {
                auto *episodeHeader = new QHBoxLayout;
                episodeHeader->setSpacing(10);
                episodeHeader->addWidget(title("Episodes", 20));
                auto *episodeCount = new QLabel;
                episodeCount->setObjectName("muted");
                episodeHeader->addWidget(episodeCount);
                episodeHeader->addStretch();
                contentLayout->addLayout(episodeHeader);
                auto *filterRow = new QHBoxLayout;
                filterRow->setSpacing(10);
                auto *seasonFilter = new QComboBox;
                seasonFilter->setMinimumHeight(40);
                seasonFilter->setMinimumWidth(135);
                seasonFilter->addItem("All seasons", 0);
                for (const auto season : CloudStream::EpisodeCatalog::seasons(episodeCatalog)) {
                    seasonFilter->addItem("Season " + QString::number(season), season);
                }
                auto *dubFilter = new QComboBox;
                dubFilter->setMinimumHeight(40);
                dubFilter->setMinimumWidth(135);
                dubFilter->addItem("All audio", QString());
                for (const auto &dubStatus : CloudStream::EpisodeCatalog::dubStatuses(episodeCatalog)) {
                    dubFilter->addItem(dubStatus, dubStatus);
                }
                auto *rangeFilter = new QComboBox;
                rangeFilter->setMinimumHeight(40);
                rangeFilter->setMinimumWidth(150);
                auto *episodeSearch = new QLineEdit;
                episodeSearch->setPlaceholderText("Find episode…");
                episodeSearch->addAction(QIcon(":/icons/search.svg"), QLineEdit::LeadingPosition);
                episodeSearch->setMinimumHeight(40);
                episodeSearch->setMaximumWidth(330);
                filterRow->addWidget(seasonFilter);
                filterRow->addWidget(dubFilter);
                filterRow->addWidget(rangeFilter);
                filterRow->addWidget(episodeSearch, 1);
                filterRow->addStretch();
                contentLayout->addLayout(filterRow);
                contentLayout->addWidget(episodes);

                auto populateEpisodes = [this, episodes, episodeCount, seasonFilter, dubFilter,
                                          rangeFilter, episodeSearch, episodeCatalog,
                                          playSelection, downloadSelection] {
                    episodes->clear();
                    const auto filtered = CloudStream::EpisodeCatalog::filter(
                        episodeCatalog, seasonFilter->currentData().toInt(),
                        dubFilter->currentData().toString(), episodeSearch->text());
                    auto offset = rangeFilter->currentData().toInt();
                    const auto maximumOffset = filtered.isEmpty() ? 0 : ((filtered.size() - 1) / 50) * 50;
                    offset = qBound(0, offset, maximumOffset);
                    {
                        const QSignalBlocker blocker(rangeFilter);
                        rangeFilter->clear();
                        if (filtered.isEmpty()) {
                            rangeFilter->addItem("No episodes", 0);
                        } else {
                            for (int start = 0; start < filtered.size(); start += 50) {
                                const auto end = std::min(start + 50, int(filtered.size()));
                                rangeFilter->addItem("Episodes " + QString::number(start + 1) + "–" +
                                                     QString::number(end), start);
                            }
                            rangeFilter->setCurrentIndex(rangeFilter->findData(offset));
                        }
                    }
                    const auto visible = CloudStream::EpisodeCatalog::page(filtered, offset, 50);
                    for (const auto &episode : visible) {
                        QString chronology;
                        if (episode.season > 0) chronology += "S" + QString::number(episode.season);
                        if (episode.number > 0) chronology += (chronology.isEmpty() ? QString() : " ") +
                            "E" + QString::number(episode.number);
                        auto episodeName = episode.name;
                        if (episodeName.isEmpty()) episodeName = episode.number > 0
                            ? "Episode " + QString::number(episode.number) : "Episode";
                        QStringList heading;
                        if (!chronology.isEmpty()) heading << chronology;
                        heading << episodeName;
                        QStringList secondary = episode.dubStatuses;
                        if (!episode.description.isEmpty()) secondary << episode.description.simplified();
                        const auto display = heading.join("  •  ") +
                            (secondary.isEmpty() ? QString() : "\n" + secondary.join("  •  "));
                        auto *item = new QListWidgetItem;
                        item->setData(Qt::UserRole, episode.data);
                        item->setData(Qt::UserRole + 1, heading.join("  •  "));
                        auto accessibleDisplay = display;
                        item->setData(Qt::AccessibleTextRole, accessibleDisplay.replace('\n', ", "));
                        item->setSizeHint(QSize(0, 66));
                        item->setToolTip(episode.description.isEmpty() ? display : episode.description);
                        episodes->addItem(item);
                        auto *row = new QWidget;
                        row->setObjectName("episodeRow");
                        auto *rowLayout = new QHBoxLayout(row);
                        rowLayout->setContentsMargins(8, 5, 6, 5);
                        rowLayout->setSpacing(7);
                        auto *rowPlay = new QToolButton;
                        rowPlay->setObjectName("episodeAction");
                        rowPlay->setIcon(QIcon(":/icons/play.svg"));
                        rowPlay->setIconSize(QSize(22, 22));
                        rowPlay->setFixedSize(38, 38);
                        rowPlay->setAccessibleName("Play " + heading.join(" "));
                        rowLayout->addWidget(rowPlay);
                        auto *body = new QPushButton(display);
                        body->setProperty("episodeBody", true);
                        auto bodyAccessible = display;
                        body->setAccessibleName("Play " + bodyAccessible.replace('\n', ", "));
                        body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                        rowLayout->addWidget(body, 1);
                        auto *rowDownload = new QToolButton;
                        rowDownload->setObjectName("episodeAction");
                        rowDownload->setIcon(QIcon(":/icons/download.svg"));
                        rowDownload->setIconSize(QSize(22, 22));
                        rowDownload->setFixedSize(38, 38);
                        rowDownload->setAccessibleName("Download " + heading.join(" "));
                        rowLayout->addWidget(rowDownload);
                        auto *more = new QToolButton;
                        more->setObjectName("episodeAction");
                        more->setIcon(QIcon(":/icons/more-vert.svg"));
                        more->setIconSize(QSize(22, 22));
                        more->setFixedSize(38, 38);
                        more->setAccessibleName("More actions for " + heading.join(" "));
                        auto *menu = new QMenu(more);
                        auto *playAction = menu->addAction(QIcon(":/icons/play.svg"), "Play in CloudStream");
                        auto *downloadAction = menu->addAction(QIcon(":/icons/download.svg"), "Download episode");
                        menu->addSeparator();
                        auto *copyAction = menu->addAction("Copy provider link");
                        auto *openAction = menu->addAction(QIcon(":/icons/open-in-new.svg"), "Open in browser");
                        const QUrl episodeUrl(episode.data);
                        openAction->setEnabled(episodeUrl.isValid() && episodeUrl.scheme().startsWith("http"));
                        more->setMenu(menu);
                        more->setPopupMode(QToolButton::InstantPopup);
                        rowLayout->addWidget(more);
                        const auto selectEpisode = [episodes, item] { episodes->setCurrentItem(item); };
                        connect(rowPlay, &QToolButton::clicked, this, [selectEpisode, playSelection] {
                            selectEpisode();
                            playSelection(false);
                        });
                        connect(body, &QPushButton::clicked, this, [selectEpisode, playSelection] {
                            selectEpisode();
                            playSelection(false);
                        });
                        connect(rowDownload, &QToolButton::clicked, this, [selectEpisode, downloadSelection] {
                            selectEpisode();
                            downloadSelection();
                        });
                        connect(playAction, &QAction::triggered, this, [selectEpisode, playSelection] {
                            selectEpisode();
                            playSelection(false);
                        });
                        connect(downloadAction, &QAction::triggered, this, [selectEpisode, downloadSelection] {
                            selectEpisode();
                            downloadSelection();
                        });
                        connect(copyAction, &QAction::triggered, this, [data = episode.data] {
                            QApplication::clipboard()->setText(data);
                        });
                        connect(openAction, &QAction::triggered, this, [episodeUrl] {
                            QDesktopServices::openUrl(episodeUrl);
                        });
                        episodes->setItemWidget(item, row);
                    }
                    episodeCount->setText(filtered.isEmpty() ? "No matches"
                        : QString::number(offset + 1) + "–" + QString::number(offset + visible.size()) +
                          " of " + QString::number(filtered.size()));
                    episodes->setFixedHeight(std::max(250, int(visible.size()) * 66 + 6));
                    if (episodes->count() > 0) episodes->setCurrentRow(0);
                };
                connect(seasonFilter, qOverload<int>(&QComboBox::currentIndexChanged), episodes,
                        [populateEpisodes](int) { populateEpisodes(); });
                connect(dubFilter, qOverload<int>(&QComboBox::currentIndexChanged), episodes,
                        [populateEpisodes](int) { populateEpisodes(); });
                connect(rangeFilter, qOverload<int>(&QComboBox::currentIndexChanged), episodes,
                        [populateEpisodes](int) { populateEpisodes(); });
                connect(episodeSearch, &QLineEdit::textChanged, episodes,
                        [populateEpisodes](const QString &) { populateEpisodes(); });
                populateEpisodes();
            }

            connect(play, &QPushButton::clicked, this, [playSelection] { playSelection(false); });
            connect(chooseSource, &QPushButton::clicked, this, [playSelection] { playSelection(true); });
            connect(download, &QPushButton::clicked, this, downloadSelection);
            connect(episodes, &QListWidget::itemActivated, this,
                    [playSelection](QListWidgetItem *) { playSelection(false); });
            connect(save, &QPushButton::clicked, this, [this, save, details, jar, provider, url,
                                                        selectedPlayback, selectedEpisodeName] {
                saveProviderHistory(details, jar, provider, url, selectedPlayback(), selectedEpisodeName());
                save->setText("In library");
                save->setIcon(QIcon(":/icons/bookmark-filled.svg"));
                status->setText("Added to Watching");
            });
            connect(sourcePage, &QPushButton::clicked, this,
                    [url] { QDesktopServices::openUrl(QUrl(url)); });

            if (!backdropUrl.isEmpty()) {
                const QPointer<HomeHeroBanner> safeHero(hero);
                artworkLoader->load(QUrl(backdropUrl), QSize(1120, 1680), hero,
                    [safeHero](const QImage &image) {
                        if (safeHero) safeHero->setBackdrop(QPixmap::fromImage(image));
                    }, CloudStream::ArtworkLoader::HighPriority,
                       CloudStream::ArtworkLoader::FitInside);
            }
            if (!posterUrl.isEmpty()) {
                const QPointer<QLabel> safePoster(poster);
                artworkLoader->load(QUrl(posterUrl), poster->size(), poster,
                    [safePoster](const QImage &image) {
                    if (safePoster && safePoster->isVisible()) {
                        safePoster->setText({});
                        safePoster->setPixmap(QPixmap::fromImage(image));
                    }
                }, CloudStream::ArtworkLoader::HighPriority);
            }

            scroll->setWidget(content);
            CloudStream::SmoothScrollController::attachRecursively(safeDialog);
            root->addWidget(scroll, 1);
            process->deleteLater();
            if (autoPlay && episodeCatalog.isEmpty() && !details.value("data").toString().isEmpty()) {
                QTimer::singleShot(0, safeDialog, [playSelection] { playSelection(false); });
            }
        });
        process->start();
        QTimer::singleShot(20000, process,
                           [process] { if (process->state() != QProcess::NotRunning) process->kill(); });
    }

    void showNetworkStreamDialog() {
        bool ok = false;
        const auto value = QInputDialog::getText(this, "Network stream", "Media URL:", QLineEdit::Normal, settings.value("lastUrl").toString(), &ok).trimmed();
        if (!ok) return;
        if (CloudStream::PlayerCommand::isValidTarget(value)) {
            settings.setValue("lastUrl", value);
            startPlayer(value);
        } else {
            QMessageBox::warning(this, "Invalid URL", "Enter a supported HTTP, HTTPS, RTMP, RTSP, magnet, or local media target.");
        }
    }

    void stopPlayerTracking() {
        if (playerProgressTimer) {
            playerProgressTimer->stop();
            playerProgressTimer = nullptr;
        }
        if (playerIpc) {
            playerIpc->disconnectFromServer();
            playerIpc->deleteLater();
            playerIpc = nullptr;
        }
        playerIpcBuffer.clear();
    }

    void persistActiveProgress() {
        if (activeHistoryId.isEmpty() || activeDurationSeconds <= 0.0) return;
        history.updateProgress(activeHistoryId, activePositionSeconds, activeDurationSeconds);
    }

    void startPlayer(const QString &target, const QString &subtitleFile = {},
                     const QString &historyId = {}) {
        playbackRequestGeneration.begin();
        if (activePlaybackResolutionProcess &&
            activePlaybackResolutionProcess->state() != QProcess::NotRunning) {
            activePlaybackResolutionProcess->kill();
        }
        if (!CloudStream::PlayerCommand::isValidTarget(target)) {
            status->setText("Invalid or missing media target");
            return;
        }
        persistActiveProgress();
        stopPlayerTracking();
        if (playerProcess && playerProcess->state() != QProcess::NotRunning) {
            playerProcess->terminate();
            if (!playerProcess->waitForFinished(1500)) {
                playerProcess->kill();
                playerProcess->waitForFinished(500);
            }
        }
        auto *process = new QProcess(this);
        playerProcess = process;
        activeHistoryId = historyId;
        activePositionSeconds = 0.0;
        activeDurationSeconds = 0.0;
        if (!historyId.isEmpty()) {
            for (const auto &entry : history.entries()) {
                if (entry.id != historyId) continue;
                if (entry.state != "Completed") activePositionSeconds = entry.positionSeconds;
                activeDurationSeconds = entry.durationSeconds;
                break;
            }
        }
        const auto watchLater = CloudStream::XdgPaths::dataDir() + "/watch-later";
        QDir().mkpath(watchLater);
        const auto socket = CloudStream::XdgPaths::cacheDir() + "/mpv-ipc-" + QString::number(QCoreApplication::applicationPid()) + ".sock";
        QFile::remove(socket);
        process->setProgram(player());
        process->setArguments(CloudStream::PlayerCommand::arguments(
            target, socket, watchLater, subtitleFile, activePositionSeconds));
        process->setProcessChannelMode(QProcess::MergedChannels);
        connect(process, &QProcess::started, this, [this, process, socket] {
            status->setText("Playback started with " + player());
            playerIpc = new QLocalSocket(process);
            connect(playerIpc, &QLocalSocket::readyRead, this, [this] {
                if (!playerIpc) return;
                playerIpcBuffer += playerIpc->readAll();
                qsizetype newline = -1;
                while ((newline = playerIpcBuffer.indexOf('\n')) >= 0) {
                    const auto line = playerIpcBuffer.left(newline);
                    playerIpcBuffer.remove(0, newline + 1);
                    const auto property = CloudStream::MpvIpcProtocol::parseResponse(line);
                    if (!property.valid) continue;
                    if (property.kind == CloudStream::MpvProperty::Position) activePositionSeconds = property.value;
                    if (property.kind == CloudStream::MpvProperty::Duration) activeDurationSeconds = property.value;
                    if (!activeHistoryId.isEmpty() && activeDurationSeconds > 0.0) {
                        history.updateProgress(activeHistoryId, activePositionSeconds, activeDurationSeconds);
                        const auto percent = qBound(0, qRound((activePositionSeconds / activeDurationSeconds) * 100.0), 100);
                        status->setText("Playing • " + QString::number(percent) + "%");
                    }
                }
            });
            auto *connectTimer = new QTimer(process);
            connectTimer->setInterval(250);
            connectTimer->setProperty("attempts", 0);
            connect(connectTimer, &QTimer::timeout, this, [this, connectTimer, socket] {
                if (!playerIpc) { connectTimer->stop(); return; }
                const auto attempts = connectTimer->property("attempts").toInt() + 1;
                connectTimer->setProperty("attempts", attempts);
                if (playerIpc->state() == QLocalSocket::ConnectedState) {
                    connectTimer->stop();
                    return;
                }
                if (QFileInfo::exists(socket) && playerIpc->state() == QLocalSocket::UnconnectedState) {
                    playerIpc->connectToServer(socket);
                }
                if (attempts >= 24) connectTimer->stop();
            });
            connect(playerIpc, &QLocalSocket::connected, this, [this, process, connectTimer] {
                connectTimer->stop();
                playerProgressTimer = new QTimer(process);
                playerProgressTimer->setInterval(5000);
                connect(playerProgressTimer, &QTimer::timeout, this, [this] {
                    if (playerIpc && playerIpc->state() == QLocalSocket::ConnectedState) {
                        playerIpc->write(CloudStream::MpvIpcProtocol::progressQuery());
                        playerIpc->flush();
                    }
                });
                playerProgressTimer->start();
                playerIpc->write(CloudStream::MpvIpcProtocol::progressQuery());
                playerIpc->flush();
            });
            connectTimer->start();
        });
        connect(process, &QProcess::readyReadStandardOutput, this, [process] { qInfo().noquote() << "mpv:" << QString::fromUtf8(process->readAllStandardOutput()).trimmed(); });
        connect(process, &QProcess::errorOccurred, this,
                [process] { qWarning() << process->errorString(); });
        CloudStream::ProcessCompletion::watch(process, this,
                [this, process, socket](int exitCode, QProcess::ExitStatus, bool startFailure) {
            QFile::remove(socket);
            status->setText(startFailure ? "Could not start the configured player"
                : exitCode == 0 ? "Playback finished" : "Player exited with an error");
            if (playerProcess == process) {
                persistActiveProgress();
                stopPlayerTracking();
                activeHistoryId.clear();
                activePositionSeconds = 0.0;
                activeDurationSeconds = 0.0;
                playerProcess = nullptr;
                refreshContinueWatching();
                if (libraryLoaded && libraryRefresh) libraryRefresh();
            }
            process->deleteLater();
        });
        process->start();
        status->setText("Starting " + player() + "…");
    }
};
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    const auto runtimeEnvironment = CloudStream::packagedRuntimeEnvironment(
        QCoreApplication::applicationDirPath(), QProcessEnvironment::systemEnvironment(), true);
    for (const auto *name : {"SSL_CERT_FILE", "CURL_CA_BUNDLE", "FONTCONFIG_FILE", "FONTCONFIG_PATH"}) {
        if (!runtimeEnvironment.value(name).isEmpty())
            CloudStream::setRuntimeEnvironmentVariable(QString::fromLatin1(name), runtimeEnvironment.value(name));
    }
#endif
    QCoreApplication::setOrganizationName("recloudstream");
    QCoreApplication::setOrganizationDomain("recloudstream.github.io");
    QCoreApplication::setApplicationName("CloudStream");
    QCoreApplication::setApplicationVersion("0.1.0");
    QGuiApplication::setDesktopFileName("io.github.recloudstream.cloudstream");
    QCommandLineParser parser;
    parser.setApplicationDescription("A native Linux client for CloudStream providers");
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption smokeOption("smoke-test", "Show one event-loop cycle without networking, then exit.");
    const QCommandLineOption platformOption("expect-platform", "Fail unless Qt uses this platform plugin.", "platform");
    const QCommandLineOption previewOption("render-preview", "Render the startup window to an image and exit.", "path");
    const QCommandLineOption previewPageOption("preview-page", "Page to render with --render-preview.", "page", "home");
    const QCommandLineOption detailsArtifactOption("details-artifact", "Provider artifact used by a details preview.", "path");
    const QCommandLineOption detailsProviderOption("details-provider", "Provider name used by a details preview.", "name");
    const QCommandLineOption detailsUrlOption("details-url", "Media URL used by a details preview.", "url");
    const QCommandLineOption sourceDataFileOption("source-data-file", "File containing provider playback data for a source preview.", "path");
    const QCommandLineOption configurationExtensionOption("configuration-extension", "Internal extension name used by a provider configuration preview.", "name");
    const QCommandLineOption settingsSectionOption("settings-section", "Settings subpage used by a settings preview.", "section");
    const QCommandLineOption playerMediaOption("player-media", "Local media file used by a player preview.", "path");
    const QCommandLineOption installAllExtensionsOption(
        "install-all-extensions",
        "Install or update every extension in an already-added repository, then exit.",
        "repository-url");
    const QCommandLineOption progressiveHomeProbeOption(
        "probe-progressive-home",
        "Scroll Home until every provider section is appended, report counts, then exit.");
    const QCommandLineOption searchProbeOption(
        "probe-search",
        "Run a provider-backed Search through the UI and report timing, then exit.",
        "query");
    parser.addOption(smokeOption);
    parser.addOption(platformOption);
    parser.addOption(previewOption);
    parser.addOption(previewPageOption);
    parser.addOption(detailsArtifactOption);
    parser.addOption(detailsProviderOption);
    parser.addOption(detailsUrlOption);
    parser.addOption(sourceDataFileOption);
    parser.addOption(configurationExtensionOption);
    parser.addOption(settingsSectionOption);
    parser.addOption(playerMediaOption);
    parser.addOption(installAllExtensionsOption);
    parser.addOption(progressiveHomeProbeOption);
    parser.addOption(searchProbeOption);
    parser.process(app);
    if (!CloudStream::XdgPaths::ensureDirectories()) {
        QMessageBox::critical(nullptr, "CloudStream Linux", "Could not create the application data directories.");
        return 1;
    }
    CloudStream::installFileLogging(CloudStream::XdgPaths::logFile());
    qInfo("CloudStream Linux starting");
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(QString(R"(
        QWidget { color:%2; font-family:'Noto Sans','DejaVu Sans',sans-serif; font-size:14px; }
        QMainWindow, QWidget#appRoot, QStackedWidget#appPages { background:%1; }
        QLabel { background:transparent; }
        QScrollArea, QScrollArea > QWidget > QWidget { background:%1; border:0; }
        #sidebar { background:#0d0e12; border:0; }
        #profileAvatar { background:#536dfe; color:white; border-radius:20px; font-weight:800; font-size:16px; }
        #card { background:%3; border:0; border-radius:16px; }
        #mediaSection { background:transparent; border:0; }
        #homeHero, #homeHero QLabel { background:transparent; border:0; }
        #homeEmptyState { background:#11131a; border:0; border-radius:18px; }
        #homeEmptyState QLabel { background:transparent; }
        #emptyStateMessage { color:#a6a8b2; font-size:14px; }
        #heroTitle { font-size:30px; font-weight:750; }
        #heroMeta { color:#c1c2ca; font-size:13px; font-weight:600; }
        QPushButton { background:#202023; color:%2; border:0; border-radius:8px; padding:0 16px; font-weight:650; }
        QPushButton:hover { background:#2b2b30; }
        QPushButton:focus { outline:0; border:2px solid #8294ff; }
        QToolButton:focus { outline:0; border:2px solid #8294ff; background:#20232d; }
        QListWidget:focus { border:1px solid #667cff; }
        QTabBar:focus { border:1px solid #667cff; border-radius:10px; }
        QSlider:focus { border:1px solid #667cff; border-radius:6px; }
        QPushButton:disabled { color:#666872; background:#1b1c21; }
        QPushButton[primary="true"] { background:#536dfe; border-color:#536dfe; color:white; }
        QPushButton[primary="true"]:hover { background:#667cff; }
        QPushButton[primary="true"]:disabled { background:#343438; color:#77777e; }
        QPushButton[danger="true"] { background:#321d22; color:#ffb4ab; }
        QPushButton[danger="true"]:hover { background:#4a252c; color:#ffd9d5; }
        QPushButton[danger="true"]:disabled { background:#211b1d; color:#705c60; }
        QPushButton[nav="true"] { background:transparent; border:0; padding:0; color:#d0d1da; border-radius:23px; }
        QPushButton[nav="true"]:hover { background:#171920; }
        QPushButton[nav="true"]:checked { background:#171c42; color:#6d82ff; border-radius:23px; }
        QPushButton[chip="true"] { min-height:34px; padding:0 16px; border-radius:17px; background:#202127; }
        QPushButton[chip="true"]:checked { background:#4f67ed; color:white; }
        QPushButton[providerPicker="true"] {
            background:#171920; border:1px solid #292d39; border-radius:20px;
            padding:0 14px; text-align:left; font-weight:680;
        }
        QPushButton[providerPicker="true"]:hover { background:#20232d; border-color:#3a4052; }
        QPushButton[providerPicker="true"]:focus { border:1px solid %4; }
        QPushButton[destructive="true"] { color:#ffb4ab; }
        QPushButton[destructive="true"]:hover { background:#3a1d22; color:#ffd7d2; }
        QPushButton[destructive="true"]:disabled { background:#1b1c21; color:#67565b; }
        QPushButton[episodeBody="true"] {
            background:transparent; border:0; border-radius:6px; padding:5px 9px;
            text-align:left; color:#eeeeF2; font-weight:600;
        }
        QPushButton[episodeBody="true"]:hover { background:#1b1b20; }
        QPushButton[settingsRow="true"] {
            min-height:58px; max-height:62px; background:transparent; border:0;
            border-radius:12px; padding:0;
        }
        QPushButton[settingsRow="true"]:hover { background:#1c1f28; }
        QPushButton[settingsRow="true"]:focus { background:#1c1f28; border:1px solid %4; }
        QPushButton[settingsRow="true"] QLabel { background:transparent; }
        QLabel#settingsRowTitle { color:#f1f1f5; font-size:15px; font-weight:650; }
        QLabel#settingsRowSummary { color:#8f929d; font-size:12px; }
        QToolButton { background:#24262d; border:0; border-radius:17px; color:%2; font-size:19px; }
        QToolButton:hover { background:#31333b; }
        QToolButton#heroIconButton, QToolButton#topIconButton { background:#1b1c22; border-radius:21px; }
        QLineEdit, QComboBox { background:%5; color:%2; border:0; border-radius:20px; padding:9px 14px; selection-background-color:%4; }
        QComboBox[providerPicker="true"] { background:#0c0d12cc; padding-left:16px; font-weight:650; }
        QComboBox::drop-down { border:0; background:transparent; width:30px; }
        QComboBox::down-arrow { image:url(:/icons/chevron-down.svg); width:14px; height:14px; }
        QLineEdit:focus, QComboBox:focus { border:1px solid %4; }
        QAbstractItemView { background:#101218; color:%2; border:1px solid #292d39; selection-background-color:#1b2148; selection-color:white; }
        QComboBox QAbstractItemView { background:#101218; color:%2; border:1px solid #292d39; }
        QLineEdit#searchField {
            background:#181b24; border:1px solid #252936; border-radius:17px;
            padding:10px 15px; font-size:16px;
        }
        QWidget#searchEmptyState, QWidget#searchHistory { background:transparent; }
        QWidget#selectionBar { background:#181b24; border:1px solid #292d39; border-radius:14px; }
        QWidget#storagePanel { background:#181b24; border:1px solid #252936; border-radius:14px; }
        QWidget#extensionDetailPanel { background:#181b24; border:1px solid #292d39; border-radius:14px; }
        QWidget#settingsProfile { background:#181b24; border:1px solid #252936; border-radius:16px; }
        QWidget#settingsList { background:transparent; }
        QDialog#extensionManagerDialog { background:%1; }
        QDialog#detailsDialog, QDialog#sourcePickerDialog { background:%1; }
        QDialog#providerConfigurationDialog { background:%1; }
        QWidget#providerPickerOverlay { background:rgba(0,0,0,224); }
        QWidget#providerPickerPanel {
            background:#08090d; border:1px solid #242834; border-radius:20px;
        }
        QWidget#providerPickerPanel QLabel#providerPickerTitle {
            color:#f4f3fa; font-size:22px; font-weight:750;
        }
        QWidget#providerPickerPanel QLabel#providerPickerSubtitle {
            color:#9da0ac; font-size:12px;
        }
        QWidget#providerPickerPanel QToolButton#providerPickerClose {
            background:#161820; border-radius:18px; font-size:23px; color:#d9dae2;
        }
        QWidget#providerPickerPanel QLineEdit#providerPickerSearch {
            background:#101218; border:1px solid #222632; border-radius:16px;
            padding:9px 14px; font-size:14px;
        }
        QWidget#providerPickerPanel QLineEdit#providerPickerSearch:focus { border:1px solid %4; }
        QListWidget#providerPickerList {
            background:#05060a; border:1px solid #1c202a; border-radius:14px;
            padding:6px;
        }
        QListWidget#providerPickerList::item { padding:0; margin:1px 0; border-radius:10px; }
        QListWidget#providerPickerList::item:selected { background:transparent; }
        QListWidget#providerPickerList::item:hover { background:#12141b; }
        QWidget#providerPickerRow { background:transparent; border-radius:10px; }
        QWidget#providerPickerRow[selected="true"] { background:#1d2754; }
        QWidget#providerPickerRow[focused="true"] { border:1px solid #6479ef; }
        QLabel#providerPickerBadge {
            background:#181b25; border-radius:18px; color:#f3f3f7;
            font-size:17px; font-weight:750;
        }
        QLabel#providerPickerRowTitle { color:#f0f0f5; font-size:14px; font-weight:700; }
        QLabel#providerPickerRowSummary { color:#9397a4; font-size:11px; }
        QLabel#providerPickerCheck { background:transparent; }
        QLabel#providerPickerSectionLabel {
            color:#8e929f; font-size:10px; font-weight:750; padding:2px 3px;
        }
        QPushButton[providerTypeChip="true"] {
            min-height:36px; padding:0 13px; border-radius:18px;
            background:#15171e; color:#c7c9d2;
        }
        QPushButton[providerTypeChip="true"]:checked { background:%4; color:white; }
        QToolButton#providerPickerPin {
            background:transparent; color:#aeb4c5; border-radius:16px; font-size:18px;
        }
        QToolButton#providerPickerPin:hover,
        QToolButton#providerPickerPin:focus { background:#2a2e3a; color:#ffd76b; }
        QListWidget#providerPickerList QScrollBar:vertical {
            background:transparent; width:8px; margin:8px 1px 8px 0;
        }
        QListWidget#providerPickerList QScrollBar::handle:vertical {
            background:#414655; border-radius:4px; min-height:42px;
        }
        QListWidget#providerPickerList QScrollBar::add-line:vertical,
        QListWidget#providerPickerList QScrollBar::sub-line:vertical { height:0; }
        QListWidget#providerPickerList QScrollBar::add-page:vertical,
        QListWidget#providerPickerList QScrollBar::sub-page:vertical { background:transparent; }
        QDialog#providerConfigurationDialog QLineEdit,
        QDialog#providerConfigurationDialog QComboBox { border:1px solid #343846; background:#181b24; }
        QDialog#providerConfigurationDialog QLineEdit:focus,
        QDialog#providerConfigurationDialog QComboBox:focus { border:1px solid %4; }
        QDialog#providerConfigurationDialog QLineEdit[invalid="true"] { border:1px solid #ff7b72; }
        QLabel#configurationError { background:#3a1d22; color:#ffb4ab; border-radius:9px; padding:9px 11px; }
        QWidget#detailsHero { background:#11131a; border-radius:18px; }
        QLabel#detailsPoster { background:#20232d; border:1px solid #333744; border-radius:12px; color:#858998; }
        QLabel#detailsFacts { color:#c0c3cc; font-weight:600; }
        QLabel#detailsTags { color:#9aa8ff; }
        QLabel#detailsPlot { color:#c5c7cf; font-size:14px; }
        QLabel#comingSoonBadge { background:#4f67ed; color:white; border-radius:12px; padding:5px 10px; font-weight:700; }
        QLabel#sourceMetadata { background:#181b24; border-radius:10px; padding:10px 12px; color:#b8bbc5; }
        QSplitter#sourceSubtitleChooser { background:transparent; }
        QSplitter#sourceSubtitleChooser::handle { background:#29292d; width:1px; }
        QWidget#sourcePane, QWidget#subtitlePane { background:transparent; }
        QLabel#settingsVersion { color:#777a85; padding:12px; }
        QLabel#selectedRepositoryLabel { color:#d8dae2; font-weight:650; background:transparent; }
        QStackedWidget { background:transparent; }
        QListWidget#historyList { background:transparent; border:0; }
        QListWidget#historyList::item { min-height:34px; padding:8px 12px; border-radius:10px; }
        QListWidget#historyList::item:hover { background:#20232d; }
        QListWidget { background:transparent; color:%2; border:0; border-radius:0; padding:0; }
        QListWidget::item { padding:10px; border-radius:8px; }
        QListWidget::item:hover { background:#17191f; }
        QListWidget::item:selected { background:#1b2148; color:white; }
        QListWidget#posterRow::item { padding:4px; margin-right:4px; }
        QListWidget#posterRow { background:#101012; border:0; border-radius:14px; }
        QListWidget#posterRow::item { background:#181b24; color:#eeeeF2; border-radius:10px; }
        QListWidget#posterRow::item:hover { background:#222632; }
        QToolButton#topIconButton { background:#181b24; border:0; border-radius:21px; color:#eeeeF2; }
        QTabBar::tab { background:transparent; color:#999ba5; padding:9px 17px; border-radius:18px; margin-right:5px; }
        QTabBar::tab:selected { background:#536dfe; color:white; font-weight:700; }
        QTabBar#libraryTabs::tab { background:#181b24; color:#c7c9d1; padding:8px 15px; border-radius:17px; }
        QTabBar#libraryTabs::tab:hover { background:#222632; color:white; }
        QTabBar#libraryTabs::tab:selected { background:%4; color:white; }
        QTabBar#downloadTabs::tab { background:#181b24; color:#c7c9d1; padding:8px 15px; border-radius:17px; }
        QTabBar#downloadTabs::tab:hover { background:#222632; color:white; }
        QTabBar#downloadTabs::tab:selected { background:%4; color:white; }
        QTabBar#extensionTabs::tab { background:#181b24; color:#c7c9d1; padding:8px 16px; border-radius:17px; }
        QTabBar#extensionTabs::tab:hover { background:#222632; color:white; }
        QTabBar#extensionTabs::tab:selected { background:%4; color:white; }
        QListWidget#downloadList::item { min-height:72px; padding:0; border-radius:10px; }
        QWidget#downloadRow { background:transparent; }
        QLabel#downloadRowTitle { color:#f4f3fa; font-size:14px; font-weight:700; background:transparent; }
        QLabel#downloadRowSummary { color:#b4b4c2; font-size:12px; background:transparent; }
        QProgressBar#downloadRowProgress { min-height:7px; max-height:7px; border-radius:3px; }
        QListWidget#extensionList::item { min-height:48px; padding:9px 12px; border-radius:10px; }
        QListWidget#episodeList::item { min-height:64px; padding:0; border-radius:7px; }
        QWidget#episodeRow { background:transparent; }
        QToolButton#episodeAction { background:transparent; border-radius:19px; }
        QToolButton#episodeAction:hover { background:#29292e; }
        QToolButton#episodeAction::menu-indicator { image:none; width:0; height:0; }
        QMenu { background:#151518; color:#eeeeF2; border:1px solid #2b2b30; padding:6px; }
        QMenu::item { padding:8px 28px 8px 12px; border-radius:5px; }
        QMenu::item:selected { background:#252a52; }
        QListWidget#sourceList::item { min-height:50px; padding:8px 10px; border-radius:10px; }
        QListWidget#subtitleList::item { min-height:46px; padding:7px 10px; border-radius:8px; }
        QProgressBar { background:#2b2d36; border:0; border-radius:5px; min-height:10px; max-height:10px; }
        QProgressBar::chunk { background:%4; border-radius:5px; }
        QStatusBar { background:#0d0e12; border:0; color:#8d8f99; }
        QScrollBar:vertical { background:transparent; width:10px; }
        QScrollBar::handle:vertical { background:#353741; border-radius:5px; min-height:40px; }
        QScrollBar:horizontal { background:transparent; height:10px; }
        QScrollBar::handle:horizontal { background:#353741; border-radius:5px; min-width:40px; }
        QScrollBar::add-line, QScrollBar::sub-line { background:transparent; border:0; }
        QScrollBar::add-page, QScrollBar::sub-page { background:transparent; }
        QToolTip { background:#151518; color:#eeeeF2; border:1px solid #2b2b30; padding:5px 8px; }
    )").arg(bg, text, surface, purple, surface2));
    defaultApplicationStyleSheet = app.styleSheet();
    if (parser.isSet(progressiveHomeProbeOption)) {
        CloudStreamWindow window(true);
        window.show();
        QEventLoop loop;
        QElapsedTimer elapsed;
        elapsed.start();
        int rendered = -1;
        int total = -1;
        int manualButtons = -1;
        qint64 longestBatchMs = -1;
        window.loadAllHomeSectionsForAutomation(
            [&](int renderedCount, int totalCount, int buttonCount,
                qint64 batchMs) {
                rendered = renderedCount;
                total = totalCount;
                manualButtons = buttonCount;
                longestBatchMs = batchMs;
                loop.quit();
            });
        loop.exec();
        const QJsonObject result{
            {"rendered", rendered},
            {"total", total},
            {"manualLoadButtons", manualButtons},
            {"longestBatchMs", longestBatchMs},
            {"elapsedMs", elapsed.elapsed()},
            {"complete", rendered > 0 && rendered == total && manualButtons == 0},
        };
        std::fprintf(stdout, "%s\n",
                     QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
        return rendered > 0 && rendered == total && manualButtons == 0 ? 0 : 5;
    }
    if (parser.isSet(searchProbeOption)) {
        CloudStreamWindow window(true);
        window.show();
        QEventLoop loop;
        int resultCount = -1;
        qint64 firstResultMs = -1;
        qint64 totalMs = -1;
        int peakProcesses = -1;
        window.searchForAutomation(parser.value(searchProbeOption),
            [&](int results, qint64 firstMs, qint64 elapsedMs, int peak) {
                resultCount = results;
                firstResultMs = firstMs;
                totalMs = elapsedMs;
                peakProcesses = peak;
                loop.quit();
            });
        loop.exec();
        const QJsonObject result{
            {"results", resultCount},
            {"firstResultMs", firstResultMs},
            {"totalMs", totalMs},
            {"peakProviderProcesses", peakProcesses},
            {"complete", resultCount >= 0 && totalMs >= 0 && peakProcesses <= 4},
        };
        std::fprintf(stdout, "%s\n",
                     QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
        return resultCount >= 0 && totalMs >= 0 && peakProcesses <= 4 ? 0 : 6;
    }
    if (parser.isSet(installAllExtensionsOption)) {
        const auto repositoryUrl = parser.value(installAllExtensionsOption).trimmed();
        QSettings commandSettings("CloudStream", "CloudStream Linux");
        commandSettings.setValue("selectedRepositoryUrl", repositoryUrl);
        CloudStreamWindow window(true);
        window.show();
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        int installed = -1;
        int failed = -1;
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(10 * 60 * 1000);
        window.installAllAvailableForAutomation([&](int installedCount, int failedCount) {
            installed = installedCount;
            failed = failedCount;
            timeout.stop();
            loop.quit();
        });
        loop.exec();
        const QJsonObject result{
            {"repository", repositoryUrl},
            {"installed", installed},
            {"failed", failed},
            {"timedOut", installed < 0},
        };
        std::fprintf(stdout, "%s\n", QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
        return installed >= 0 && failed == 0 ? 0 : 4;
    }
    if (parser.isSet(previewOption)) {
        CloudStreamWindow window(true);
        window.show();
        QWidget *previewTarget = &window;
        const auto previewPage = parser.value(previewPageOption).toLower();
        const bool detailsPreview = previewPage == "details";
        const bool sourcePreview = previewPage == "sources" || previewPage == "download-sources";
        const bool downloadSourcePreview = previewPage == "download-sources";
        const bool playerPreview = previewPage == "player";
        const bool configurationPreview = previewPage == "provider-configuration";
        const bool providerPickerPreview = previewPage == "provider-picker";
        const auto path = parser.value(previewOption);
        const QStringList previewPages{"home", "search", "library", "downloads", "settings"};
        if (playerPreview) {
            window.openPlayerForPreview(parser.value(playerMediaOption), path);
        } else if (configurationPreview) {
            window.openProviderConfigurationForPreview(
                parser.value(configurationExtensionOption), path);
        } else if (sourcePreview) {
            QFile dataFile(parser.value(sourceDataFileOption));
            if (!dataFile.open(QIODevice::ReadOnly)) {
                std::fprintf(stderr, "PREVIEW_FAILED could not read source data\n");
                return 3;
            }
            if (downloadSourcePreview) {
                window.openDownloadSourcesForPreview(parser.value(detailsArtifactOption),
                    parser.value(detailsProviderOption), QString::fromUtf8(dataFile.readAll()), path);
            } else {
                window.openSourcesForPreview(parser.value(detailsArtifactOption),
                    parser.value(detailsProviderOption), QString::fromUtf8(dataFile.readAll()), path);
            }
        } else if (detailsPreview) {
            window.openDetailsForPreview(parser.value(detailsArtifactOption),
                                         parser.value(detailsProviderOption),
                                         parser.value(detailsUrlOption));
        } else if (previewPage.startsWith("extensions")) {
            const int tab = previewPage == "extensions-downloaded" ? 1
                          : previewPage == "extensions-available" ? 2 : 0;
            previewTarget = window.openExtensionsForPreview(tab);
        } else {
            const auto pageIndex = previewPages.indexOf(previewPage);
            window.selectPage(pageIndex < 0 ? 0 : pageIndex);
            if (previewPage == "settings" && parser.isSet(settingsSectionOption)) {
                window.openSettingsSectionForPreview(parser.value(settingsSectionOption));
            }
        }
        QEventLoop settle;
        QTimer::singleShot(sourcePreview ? 20000 : providerPickerPreview ? 12000
                                              : playerPreview ? 3500 : 6000,
                           &settle, &QEventLoop::quit);
        settle.exec();
        if (providerPickerPreview) {
            previewTarget = window.openProviderPickerForPreview();
            QEventLoop pickerSettle;
            QTimer::singleShot(700, &pickerSettle, &QEventLoop::quit);
            pickerSettle.exec();
        }
        if (detailsPreview) {
            for (auto *widget : QApplication::topLevelWidgets()) {
                if (widget->objectName() == "detailsDialog") {
                    previewTarget = widget;
                    break;
                }
            }
        }
        const auto saved = (sourcePreview || configurationPreview || playerPreview) ? QFileInfo::exists(path)
                                         : previewTarget && previewTarget->grab().save(path);
        std::fprintf(stdout, "PREVIEW_%s path=%s\n", saved ? "OK" : "FAILED", path.toUtf8().constData());
        return saved ? 0 : 3;
    }
    if (parser.isSet(smokeOption)) {
        CloudStreamWindow window(false);
        window.show();
        app.processEvents();
        const auto expected = parser.value(platformOption);
        const auto actual = QGuiApplication::platformName();
        if (!expected.isEmpty() && expected != actual) {
            std::fprintf(stderr, "SMOKE_TEST_FAILED expected=%s actual=%s\n",
                         expected.toUtf8().constData(), actual.toUtf8().constData());
            return 2;
        }
        std::fprintf(stdout, "SMOKE_TEST_OK platform=%s\n", actual.toUtf8().constData());
        return 0;
    }
    QPixmap splashCanvas(520, 320);
    splashCanvas.fill(QColor(bg));
    {
        QPainter painter(&splashCanvas);
        const auto logo = QPixmap(":/assets/cloudstream.svg").scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap((splashCanvas.width() - logo.width()) / 2, 52, logo);
        painter.setPen(QColor(text));
        QFont brandFont = app.font();
        brandFont.setPointSize(18);
        brandFont.setBold(true);
        painter.setFont(brandFont);
        painter.drawText(QRect(0, 205, splashCanvas.width(), 38), Qt::AlignCenter, "CLOUDSTREAM");
        painter.setPen(QColor(muted));
        QFont statusFont = app.font();
        statusFont.setPointSize(10);
        painter.setFont(statusFont);
        painter.drawText(QRect(0, 245, splashCanvas.width(), 28), Qt::AlignCenter, "Loading repositories and local library…");
    }
    QSplashScreen splash(splashCanvas);
    splash.show();
    app.processEvents();
    CloudStreamWindow window;
    QTimer::singleShot(450, &window, [&window, &splash] {
        window.show();
        splash.finish(&window);
    });
    return app.exec();
}
