#include "SettingsPane.h"
#include "../storage/XdgPaths.h"
#include "../ui/SmoothScrollController.h"

#include <QAbstractButton>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>

namespace CloudStream {
namespace {
constexpr auto accent = "#536dfe";
constexpr auto secondary = "#a5a5ad";

class MaterialSwitch final : public QAbstractButton {
public:
    explicit MaterialSwitch(bool checked, QWidget *parent = nullptr) : QAbstractButton(parent) {
        setCheckable(true);
        setChecked(checked);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setFixedSize(44, 26);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF track(1, 4, 42, 18);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(isChecked() ? accent : "#55555b"));
        painter.drawRoundedRect(track, 9, 9);
        painter.setBrush(QColor(isEnabled() ? "#f5f5f7" : "#99999f"));
        const qreal x = isChecked() ? 23.0 : 3.0;
        painter.drawEllipse(QRectF(x, 3, 20, 20));
        if (hasFocus()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor("#9aacff"), 1.5));
            painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 12, 12);
        }
    }
};

QLabel *heading(const QString &text, int size = 23) {
    auto *label = new QLabel(text);
    label->setStyleSheet(QString("font-size:%1px;font-weight:700;color:#f3f3f5;background:transparent;").arg(size));
    return label;
}

QLabel *sectionLabel(const QString &text) {
    auto *label = new QLabel(text);
    label->setObjectName("settingsSectionLabel");
    label->setStyleSheet("color:#6f86ff;font-size:13px;font-weight:700;padding:13px 12px 5px 12px;background:transparent;");
    return label;
}

QLabel *iconLabel(const QString &icon) {
    auto *label = new QLabel;
    label->setFixedSize(32, 32);
    label->setAlignment(Qt::AlignCenter);
    label->setPixmap(QIcon(icon).pixmap(QSize(23, 23)));
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    return label;
}

QVBoxLayout *copyLayout(const QString &title, const QString &summary, QWidget *parent) {
    auto *copy = new QVBoxLayout;
    copy->setContentsMargins(0, 0, 0, 0);
    copy->setSpacing(2);
    auto *titleLabel = new QLabel(title, parent);
    titleLabel->setObjectName("preferenceTitle");
    titleLabel->setStyleSheet("color:#f0f0f3;font-size:15px;font-weight:600;background:transparent;");
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *summaryLabel = new QLabel(summary, parent);
    summaryLabel->setObjectName("preferenceSummary");
    summaryLabel->setStyleSheet("color:#96969f;font-size:12px;background:transparent;");
    summaryLabel->setWordWrap(true);
    summaryLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    copy->addWidget(titleLabel);
    copy->addWidget(summaryLabel);
    return copy;
}

QPushButton *preferenceButton(const QString &icon, const QString &title,
                              const QString &summary, const QString &value = {}) {
    auto *row = new QPushButton;
    row->setObjectName("preferenceRow");
    row->setProperty("settingsRow", true);
    row->setAccessibleName(title);
    row->setAccessibleDescription(summary + (value.isEmpty() ? QString() : ", " + value));
    row->setMinimumHeight(summary.isEmpty() ? 54 : 66);
    row->setMaximumHeight(summary.isEmpty() ? 58 : 76);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 7, 12, 7);
    layout->setSpacing(14);
    layout->addWidget(iconLabel(icon));
    layout->addLayout(copyLayout(title, summary, row), 1);
    if (!value.isEmpty()) {
        auto *valueLabel = new QLabel(value, row);
        valueLabel->setObjectName("preferenceValue");
        valueLabel->setStyleSheet("color:#b7b7bf;font-size:12px;background:transparent;");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(valueLabel);
    }
    auto *chevron = iconLabel(":/icons/chevron-right.svg");
    chevron->setFixedSize(24, 24);
    layout->addWidget(chevron);
    return row;
}

QWidget *toggleRow(QSettings *settings, const QString &icon, const QString &title,
                   const QString &summary, const QString &key, bool defaultValue,
                   QObject *context, const std::function<void(const QString &)> &changed,
                   const std::function<void(const QString &)> &status) {
    auto *row = new QWidget;
    row->setObjectName("preferenceToggleRow");
    row->setMinimumHeight(66);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 7, 12, 7);
    layout->setSpacing(14);
    layout->addWidget(iconLabel(icon));
    layout->addLayout(copyLayout(title, summary, row), 1);
    auto *toggle = new MaterialSwitch(settings->value(key, defaultValue).toBool(), row);
    toggle->setAccessibleName(title);
    layout->addWidget(toggle);
    QObject::connect(toggle, &QAbstractButton::toggled, context,
                     [settings, key, title, changed, status](bool enabled) {
        settings->setValue(key, enabled);
        settings->sync();
        changed(key);
        status(title + (enabled ? " enabled" : " disabled"));
    });
    return row;
}

QScrollArea *scrollFor(QWidget *content) {
    content->setMaximumWidth(900);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    scroll->setWidget(content);
    SmoothScrollController::attach(scroll);
    return scroll;
}
} // namespace

SettingsPane::SettingsPane(QSettings *settings, QWidget *parent)
    : QWidget(parent), settings_(settings) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    stack_ = new QStackedWidget;
    layout->addWidget(stack_);
    stack_->addWidget(buildOverview());
}

QString SettingsPane::currentSection() const {
    return currentSection_;
}

void SettingsPane::showOverview() {
    currentSection_ = "Settings";
    stack_->setCurrentIndex(0);
}

void SettingsPane::showSection(const QString &section) {
    if (section == "Extensions") {
        emit extensionsRequested();
        return;
    }
    while (stack_->count() > 1) {
        auto *old = stack_->widget(1);
        stack_->removeWidget(old);
        old->deleteLater();
    }
    stack_->addWidget(buildSection(section));
    currentSection_ = section;
    stack_->setCurrentIndex(1);
}

QWidget *SettingsPane::buildOverview() {
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(30, 26, 30, 28);
    layout->setSpacing(12);
    layout->addWidget(heading("Settings"));

    auto *profile = new QPushButton;
    profile->setObjectName("settingsProfile");
    profile->setProperty("profileCard", true);
    profile->setMinimumHeight(76);
    auto *profileLayout = new QHBoxLayout(profile);
    profileLayout->setContentsMargins(16, 10, 16, 10);
    profileLayout->setSpacing(13);
    auto *logo = new QLabel;
    logo->setPixmap(QPixmap(":/assets/cloudstream-launcher.png").scaled(
        48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setFixedSize(50, 50);
    logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    profileLayout->addWidget(logo);
    profileLayout->addLayout(copyLayout(settings_->value("profileName", "Standard").toString(),
                                         "Local CloudStream profile", profile), 1);
    profileLayout->addWidget(iconLabel(":/icons/chevron-right.svg"));
    connect(profile, &QPushButton::clicked, this, [this] { showSection("Accounts and security"); });
    layout->addWidget(profile);

    struct Category { QString icon; QString name; QString summary; };
    const QList<Category> categories = {
        {":/icons/settings-outline.svg", "General", "Language, downloads, storage and diagnostics"},
        {":/icons/play.svg", "Player", "Seeking, quality, volume and source fallback"},
        {":/icons/library-outline.svg", "Providers", "Languages, media sources and provider choices"},
        {":/icons/tune.svg", "Interface", "Android-dark appearance, density and poster sizing"},
        {":/icons/download.svg", "Updates and backup", "Updates, export, restore and logs"},
        {":/icons/account.svg", "Accounts and security", "Local profile and synchronization services"},
        {":/icons/extension.svg", "Extensions", "Repositories, installed providers and configuration"},
    };
    for (const auto &category : categories) {
        auto *row = preferenceButton(category.icon, category.name, category.summary);
        row->setObjectName("settingsCategory");
        row->setProperty("section", category.name);
        connect(row, &QPushButton::clicked, this, [this, name = category.name] { showSection(name); });
        layout->addWidget(row);
    }
    auto *version = new QLabel("CloudStream Linux 0.1.0  •  native Qt 6");
    version->setObjectName("settingsVersion");
    version->setAlignment(Qt::AlignCenter);
    version->setStyleSheet("color:#73737b;padding:14px;background:transparent;");
    layout->addWidget(version);
    layout->addStretch();
    return scrollFor(content);
}

QWidget *SettingsPane::buildSection(const QString &section) {
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(24, 20, 24, 24);
    pageLayout->setSpacing(10);
    auto *header = new QHBoxLayout;
    auto *back = new QPushButton("←");
    back->setObjectName("settingsBack");
    back->setAccessibleName("Back to Settings");
    back->setFixedSize(42, 42);
    back->setStyleSheet("font-size:23px;padding:0;");
    connect(back, &QPushButton::clicked, this, &SettingsPane::showOverview);
    header->addWidget(back);
    header->addWidget(heading(section));
    header->addStretch();
    pageLayout->addLayout(header);

    auto *content = new QWidget;
    content->setObjectName("settingsSectionContent");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 0, 12, 18);
    layout->setSpacing(2);
    const auto status = [this](const QString &message) { emit statusMessage(message); };
    const auto changed = [this](const QString &key) { emit settingChanged(key); };
    const auto addAction = [this, layout](const QString &icon, const QString &title,
                                         const QString &summary, const QString &value,
                                         const std::function<void()> &action) {
        auto *row = preferenceButton(icon, title, summary, value);
        connect(row, &QPushButton::clicked, this, action);
        layout->addWidget(row);
    };
    const auto addToggle = [this, layout, changed, status](const QString &icon, const QString &title,
                                                           const QString &summary, const QString &key,
                                                           bool defaultValue) {
        layout->addWidget(toggleRow(settings_, icon, title, summary, key, defaultValue,
                                    this, changed, status));
    };

    if (section == "General") {
        layout->addWidget(sectionLabel("General"));
        addAction(":/icons/settings-outline.svg", "App language",
                  "Language used by the desktop interface", settings_->value("language", "System default").toString(),
                  [this, status, changed] {
            bool ok = false;
            const QStringList options{"System default", "English", "Deutsch"};
            const auto current = options.indexOf(settings_->value("language", options.first()).toString());
            const auto choice = QInputDialog::getItem(this, "App language", "Language:", options,
                                                       qMax(0, current), false, &ok);
            if (!ok) return;
            settings_->setValue("language", choice);
            settings_->sync();
            changed("language");
            status("Language saved; restart CloudStream to apply it everywhere");
            showSection("General");
        });
        const auto downloadFolder = settings_->value("downloadFolder", QDir::homePath() + "/Downloads").toString();
        addAction(":/icons/folder-open.svg", "Download directory",
                  "Movies and episodes are stored here", downloadFolder,
                  [this, downloadFolder, status, changed] {
            const auto path = QFileDialog::getExistingDirectory(this, "Choose download directory", downloadFolder);
            if (path.isEmpty()) return;
            settings_->setValue("downloadFolder", path);
            settings_->sync();
            changed("downloadFolder");
            status("Download directory updated");
            showSection("General");
        });
        addAction(":/icons/open-in-new.svg", "External player executable",
                  "Fallback player used when embedded playback is unavailable",
                  settings_->value("player", "mpv").toString(), [this, status, changed] {
            bool ok = false;
            const auto value = QInputDialog::getText(this, "External player", "Executable:",
                QLineEdit::Normal, settings_->value("player", "mpv").toString(), &ok).trimmed();
            if (!ok || value.isEmpty()) return;
            settings_->setValue("player", value);
            settings_->sync();
            changed("player");
            status("External player saved");
            showSection("General");
        });
        layout->addWidget(sectionLabel("Storage and diagnostics"));
        addAction(":/icons/folder-open.svg", "Application data", CloudStream::XdgPaths::dataDir(), {},
                  [status] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(CloudStream::XdgPaths::dataDir()));
            status("Opened application data folder");
        });
        addAction(":/icons/refresh.svg", "Clear image cache",
                  "Remove cached poster and backdrop images", {}, [this, status] {
            if (QMessageBox::question(this, "Clear image cache",
                    "Remove all cached artwork? It will be downloaded again when needed.") != QMessageBox::Yes) return;
            QDir cache(CloudStream::XdgPaths::cacheDir() + "/images");
            cache.removeRecursively();
            QDir().mkpath(cache.path());
            status("Image cache cleared");
        });
        addAction(":/icons/github.svg", "CloudStream on GitHub",
                  "Source code, releases and issue tracker", {}, [] {
            QDesktopServices::openUrl(QUrl("https://github.com/recloudstream/cloudstream"));
        });
    } else if (section == "Player") {
        layout->addWidget(sectionLabel("Playback"));
        addAction(":/icons/speed.svg", "Seek interval",
                  "Seconds used by rewind and forward controls",
                  QString::number(settings_->value("player/seekSeconds", 10).toInt()) + " seconds",
                  [this, status, changed] {
            bool ok = false;
            const auto value = QInputDialog::getInt(this, "Seek interval", "Seconds:",
                settings_->value("player/seekSeconds", 10).toInt(), 5, 90, 5, &ok);
            if (!ok) return;
            settings_->setValue("player/seekSeconds", value);
            settings_->sync();
            changed("player/seekSeconds");
            status("Seek interval updated");
            showSection("Player");
        });
        addAction(":/icons/volume-up.svg", "Default volume", "Initial embedded-player volume",
                  QString::number(settings_->value("player/volume", 80).toInt()) + "%",
                  [this, status, changed] {
            bool ok = false;
            const auto value = QInputDialog::getInt(this, "Default volume", "Volume:",
                settings_->value("player/volume", 80).toInt(), 0, 100, 5, &ok);
            if (!ok) return;
            settings_->setValue("player/volume", value);
            settings_->sync();
            changed("player/volume");
            status("Default volume updated");
            showSection("Player");
        });
        const QStringList qualityOptions{"Automatic (best)", "1080p", "720p", "480p", "360p"};
        addAction(":/icons/hd.svg", "Preferred quality",
                  "Initial source selected before fallback", settings_->value("player/quality", qualityOptions.first()).toString(),
                  [this, qualityOptions, status, changed] {
            bool ok = false;
            const auto saved = settings_->value("player/quality", qualityOptions.first()).toString();
            const auto choice = QInputDialog::getItem(this, "Preferred quality", "Quality:", qualityOptions,
                qMax(0, qualityOptions.indexOf(saved)), false, &ok);
            if (!ok) return;
            settings_->setValue("player/quality", choice);
            settings_->sync();
            changed("player/quality");
            status("Preferred quality updated");
            showSection("Player");
        });
        addToggle(":/icons/source-selector.svg", "Automatic source fallback",
                  "Try the next resolved hoster when playback fails", "player/automaticFallback", true);
        addToggle(":/icons/info.svg", "Show player information",
                  "Display title, source and quality above the video", "player/showInformation", true);
        layout->addWidget(sectionLabel("Subtitles"));
        addAction(":/icons/subtitles.svg", "Subtitle default",
                  "Initial subtitle mode for embedded playback",
                  settings_->value("player/subtitles", "Off").toString(), [this, status, changed] {
            bool ok = false;
            const QStringList choices{"Off", "First available"};
            const auto saved = settings_->value("player/subtitles", "Off").toString();
            const auto choice = QInputDialog::getItem(this, "Subtitle default", "Mode:", choices,
                qMax(0, choices.indexOf(saved)), false, &ok);
            if (!ok) return;
            settings_->setValue("player/subtitles", choice);
            settings_->sync();
            changed("player/subtitles");
            status("Subtitle default updated");
            showSection("Player");
        });
    } else if (section == "Providers") {
        layout->addWidget(sectionLabel("Provider selection"));
        addAction(":/icons/home-outline.svg", "Home provider",
                  "Choose the single provider that fills Home", {}, [this] { emit homeProviderRequested(); });
        addAction(":/icons/search.svg", "Search providers",
                  "Choose one or several providers for search", {}, [this] { emit searchProvidersRequested(); });
        addAction(":/icons/extension.svg", "Installed extensions",
                  "Enable, disable and configure provider packages", {}, [this] { emit extensionsRequested(); });
        layout->addWidget(sectionLabel("Content"));
        const QStringList languages{"All languages", "German", "English", "Italian", "Spanish", "French", "Vietnamese"};
        addAction(":/icons/account.svg", "Provider language",
                  "Limit automatic and Random provider choices", settings_->value("providers/language", languages.first()).toString(),
                  [this, languages, status, changed] {
            bool ok = false;
            const auto saved = settings_->value("providers/language", languages.first()).toString();
            const auto choice = QInputDialog::getItem(this, "Provider language", "Language:", languages,
                qMax(0, languages.indexOf(saved)), false, &ok);
            if (!ok) return;
            settings_->setValue("providers/language", choice);
            settings_->sync();
            changed("providers/language");
            status("Provider language updated");
            showSection("Providers");
        });
        addToggle(":/icons/warning.svg", "Show adult providers",
                  "Show NSFW providers in Home, Search, and Random choices",
                  "providers/allowNsfw", false);
        addToggle(":/icons/volume-up.svg", "Mute adult-provider playback on start",
                  "Start videos from NSFW providers at 0% volume",
                  "providers/muteNsfwByDefault", true);
    } else if (section == "Interface") {
        layout->addWidget(sectionLabel("Appearance"));
        const QStringList themes{"Android dark", "AMOLED black", "Graphite", "High contrast"};
        addAction(":/icons/tune.svg", "App theme", "Colors used throughout CloudStream",
                  settings_->value("interface/theme", themes.first()).toString(),
                  [this, themes, status, changed] {
            bool ok = false;
            const auto saved = settings_->value("interface/theme", themes.first()).toString();
            const auto choice = QInputDialog::getItem(this, "App theme", "Theme:", themes,
                qMax(0, themes.indexOf(saved)), false, &ok);
            if (!ok) return;
            settings_->setValue("interface/theme", choice);
            settings_->sync();
            changed("interface/theme");
            status("App theme changed");
            showSection("Interface");
        });
        const QStringList layouts{"Compact side rail", "Expanded side navigation", "Focus content"};
        addAction(":/icons/settings-outline.svg", "App layout",
                  "Choose compact, expanded, or distraction-free navigation",
                  settings_->value("interface/layout", layouts.first()).toString(),
                  [this, layouts, status, changed] {
            bool ok = false;
            const auto saved = settings_->value("interface/layout", layouts.first()).toString();
            const auto choice = QInputDialog::getItem(this, "App layout", "Layout:", layouts,
                qMax(0, layouts.indexOf(saved)), false, &ok);
            if (!ok) return;
            settings_->setValue("interface/layout", choice);
            settings_->sync();
            changed("interface/layout");
            status("App layout changed");
            showSection("Interface");
        });
        const QStringList densities{"Compact", "Comfortable", "Spacious", "TV / 10-foot"};
        addAction(":/icons/tune.svg", "Layout density", "Spacing used by lists and controls",
                  settings_->value("interface/density", "Comfortable").toString(),
                  [this, densities, status, changed] {
            bool ok = false;
            const auto saved = settings_->value("interface/density", "Comfortable").toString();
            const auto choice = QInputDialog::getItem(this, "Layout density", "Density:", densities,
                qMax(0, densities.indexOf(saved)), false, &ok);
            if (!ok) return;
            settings_->setValue("interface/density", choice);
            settings_->setValue("compact", choice == "Compact");
            settings_->sync();
            changed("interface/density");
            status("Layout density changed");
            showSection("Interface");
        });
        const QStringList windowModes{"Separate windows", "Single-window navigation"};
        addAction(":/icons/open-in-new.svg", "Window mode",
                  "Open details and playback in separate windows or inside this window",
                  settings_->value("interface/windowMode", windowModes.first()).toString(),
                  [this, windowModes, status, changed] {
            bool ok = false;
            const auto saved = settings_->value("interface/windowMode", windowModes.first()).toString();
            const auto choice = QInputDialog::getItem(this, "Window mode", "Mode:", windowModes,
                qMax(0, windowModes.indexOf(saved)), false, &ok);
            if (!ok) return;
            settings_->setValue("interface/windowMode", choice);
            settings_->sync();
            changed("interface/windowMode");
            status("Window mode changed for the next details or playback view");
            showSection("Interface");
        });
        const QStringList posterSizes{"Small (120 px)", "Compact (140 px)", "Standard (160 px)",
                                     "Large (190 px)", "Extra large (220 px)"};
        addAction(":/icons/library-outline.svg", "Poster width",
                  "Card width used in media grids and shelves", settings_->value(
                      "interface/posterSize", posterSizes[2]).toString(),
                  [this, posterSizes, status, changed] {
            bool ok = false;
            const auto saved = settings_->value("interface/posterSize", posterSizes[2]).toString();
            const auto choice = QInputDialog::getItem(this, "Poster width", "Size:", posterSizes,
                qMax(0, posterSizes.indexOf(saved)), false, &ok);
            if (!ok) return;
            settings_->setValue("interface/posterSize", choice);
            settings_->sync();
            changed("interface/posterSize");
            status("Poster width saved for the next launch");
            showSection("Interface");
        });
    } else if (section == "Updates and backup") {
        layout->addWidget(sectionLabel("Updates"));
        addAction(":/icons/refresh.svg", "Check for CloudStream updates",
                  "Open the project releases page", {}, [] {
            QDesktopServices::openUrl(QUrl("https://github.com/recloudstream/cloudstream/releases"));
        });
        addAction(":/icons/extension.svg", "Update extensions",
                  "Refresh repository metadata and available versions", {}, [this] { emit extensionsRequested(); });
        layout->addWidget(sectionLabel("Backup"));
        addAction(":/icons/download.svg", "Back up settings",
                  "Export desktop preferences to an INI file", {}, [this, status] {
            const auto target = QFileDialog::getSaveFileName(this, "Back up settings",
                QDir::homePath() + "/cloudstream-linux-settings.ini", "Settings (*.ini)");
            if (target.isEmpty()) return;
            settings_->sync();
            QFile::remove(target);
            status(QFile::copy(settings_->fileName(), target) ? "Settings backup created" : "Settings backup failed");
        });
        addAction(":/icons/refresh.svg", "Restore settings",
                  "Import a CloudStream Linux settings backup", {}, [this, status] {
            const auto source = QFileDialog::getOpenFileName(this, "Restore settings", QDir::homePath(),
                                                              "Settings (*.ini *.conf)");
            if (source.isEmpty()) return;
            if (QMessageBox::question(this, "Restore settings",
                    "Replace current desktop settings with this backup?") != QMessageBox::Yes) return;
            QSettings imported(source, QSettings::IniFormat);
            settings_->clear();
            for (const auto &key : imported.allKeys()) settings_->setValue(key, imported.value(key));
            settings_->sync();
            status("Settings restored; restart CloudStream to apply every change");
        });
        addAction(":/icons/open-in-new.svg", "Open log file", CloudStream::XdgPaths::logFile(), {}, [status] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(CloudStream::XdgPaths::logFile()));
            status("Opened CloudStream log");
        });
    } else if (section == "Accounts and security") {
        layout->addWidget(sectionLabel("Local profile"));
        addAction(":/icons/account.svg", "Profile name", "Name shown in CloudStream",
                  settings_->value("profileName", "Standard").toString(), [this, status] {
            bool ok = false;
            const auto current = settings_->value("profileName", "Standard").toString();
            const auto name = QInputDialog::getText(this, "Profile name", "Name:",
                                                     QLineEdit::Normal, current, &ok).trimmed();
            if (!ok || name.isEmpty()) return;
            settings_->setValue("profileName", name);
            settings_->sync();
            emit profileNameChanged(name);
            status("Profile renamed");
            showSection("Accounts and security");
        });
        layout->addWidget(sectionLabel("Sync services"));
        const QStringList services{"MAL", "Kitsu", "AniList", "Simkl", "OpenSubtitles", "SubDL", "AnimeSkip"};
        for (const auto &service : services) {
            auto *row = new QWidget;
            row->setObjectName("accountServiceRow");
            row->setToolTip("Desktop sign-in is not implemented for " + service);
            row->setMinimumHeight(58);
            auto *serviceLayout = new QHBoxLayout(row);
            serviceLayout->setContentsMargins(12, 6, 12, 6);
            auto *badge = new QLabel(service.left(1));
            badge->setAlignment(Qt::AlignCenter);
            badge->setFixedSize(30, 30);
            badge->setStyleSheet("background:#242429;border-radius:15px;color:#e8e8ec;font-weight:700;");
            serviceLayout->addWidget(badge);
            serviceLayout->addLayout(copyLayout(service, "Not connected • desktop sign-in is not implemented", row), 1);
            auto *state = new QLabel("Unavailable");
            state->setStyleSheet("color:#73737b;background:transparent;");
            serviceLayout->addWidget(state);
            layout->addWidget(row);
        }
        layout->addWidget(sectionLabel("Security"));
        auto *security = new QWidget;
        security->setMinimumHeight(62);
        auto *securityLayout = new QHBoxLayout(security);
        securityLayout->setContentsMargins(12, 6, 12, 6);
        securityLayout->addWidget(iconLabel(":/icons/info.svg"));
        securityLayout->addLayout(copyLayout("Remote credentials", "No remote account token is stored by this Linux client", security), 1);
        layout->addWidget(security);
    } else {
        layout->addWidget(sectionLabel("Unavailable"));
        auto *message = new QLabel("This settings section is not available in the Linux build yet.");
        message->setStyleSheet(QString("color:%1;padding:12px;").arg(secondary));
        layout->addWidget(message);
    }
    layout->addStretch();
    pageLayout->addWidget(scrollFor(content), 1);
    return page;
}

} // namespace CloudStream
