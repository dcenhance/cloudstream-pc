#pragma once

#include <QWidget>

class QSettings;
class QStackedWidget;

namespace CloudStream {

namespace Updates { class ReleaseUpdater; }

class SettingsPane final : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPane(QSettings *settings, QWidget *parent = nullptr);

    QString currentSection() const;
    void showSection(const QString &section);
    void showOverview();

signals:
    void extensionsRequested();
    void homeProviderRequested();
    void searchProvidersRequested();
    void profileNameChanged(const QString &name);
    void settingChanged(const QString &key);
    void statusMessage(const QString &message);

private:
    QWidget *buildOverview();
    QWidget *buildSection(const QString &section);

    Updates::ReleaseUpdater *updater_{};
    QSettings *settings_{};
    QStackedWidget *stack_{};
    QString currentSection_ = "Settings";
};

} // namespace CloudStream
