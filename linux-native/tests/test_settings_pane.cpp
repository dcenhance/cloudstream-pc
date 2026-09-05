#include <QtTest>

#include "../settings/SettingsPane.h"

#include <QPushButton>
#include <QAbstractButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

class SettingsPaneTest final : public QObject {
    Q_OBJECT
private slots:
    void exposesSevenAndroidStyleCategories() {
        QTemporaryDir temporary;
        QSettings settings(temporary.filePath("settings.ini"), QSettings::IniFormat);
        CloudStream::SettingsPane pane(&settings);
        const auto categories = pane.findChildren<QPushButton *>("settingsCategory");
        QCOMPARE(categories.size(), 7);
        QStringList sections;
        for (auto *category : categories) sections << category->property("section").toString();
        QCOMPARE(sections, QStringList({"General", "Player", "Providers", "Interface",
                                       "Updates and backup", "Accounts and security", "Extensions"}));
        QCOMPARE(pane.currentSection(), QString("Settings"));
    }

    void navigatesIntoSectionAndBackWithoutReplacingOuterNavigation() {
        QTemporaryDir temporary;
        QSettings settings(temporary.filePath("settings.ini"), QSettings::IniFormat);
        CloudStream::SettingsPane pane(&settings);
        pane.showSection("General");
        QCOMPARE(pane.currentSection(), QString("General"));
        auto *back = pane.findChild<QPushButton *>("settingsBack");
        QVERIFY(back);
        back->click();
        QCOMPARE(pane.currentSection(), QString("Settings"));
    }

    void extensionsCategoryUsesRealRoutingSignal() {
        QTemporaryDir temporary;
        QSettings settings(temporary.filePath("settings.ini"), QSettings::IniFormat);
        CloudStream::SettingsPane pane(&settings);
        QSignalSpy opened(&pane, &CloudStream::SettingsPane::extensionsRequested);
        auto categories = pane.findChildren<QPushButton *>("settingsCategory");
        auto *extensions = *std::find_if(categories.begin(), categories.end(), [](auto *category) {
            return category->property("section").toString() == "Extensions";
        });
        extensions->click();
        QCOMPARE(opened.count(), 1);
    }

    void accountSectionListsTruthfulDesktopServiceStates() {
        QTemporaryDir temporary;
        QSettings settings(temporary.filePath("settings.ini"), QSettings::IniFormat);
        CloudStream::SettingsPane pane(&settings);
        pane.showSection("Accounts and security");
        const auto services = pane.findChildren<QWidget *>("accountServiceRow");
        QCOMPARE(services.size(), 7);
        for (auto *service : services) {
            QVERIFY(service->toolTip().contains("not implemented", Qt::CaseInsensitive));
        }
    }

    void interfaceSectionExposesWorkingAppearanceAndWindowChoices() {
        QTemporaryDir temporary;
        QSettings settings(temporary.filePath("settings.ini"), QSettings::IniFormat);
        CloudStream::SettingsPane pane(&settings);
        pane.showSection("Interface");

        const auto rows = pane.findChildren<QPushButton *>("preferenceRow");
        QStringList names;
        for (auto *row : rows) names << row->accessibleName();
        QVERIFY(names.contains("App theme"));
        QVERIFY(names.contains("App layout"));
        QVERIFY(names.contains("Layout density"));
        QVERIFY(names.contains("Window mode"));
        QVERIFY(names.contains("Poster width"));
    }

    void providerSectionExposesAdultPlaybackMuteDefault() {
        QTemporaryDir temporary;
        QSettings settings(temporary.filePath("settings.ini"), QSettings::IniFormat);
        CloudStream::SettingsPane pane(&settings);
        pane.showSection("Providers");

        QAbstractButton *mute = nullptr;
        for (auto *candidate : pane.findChildren<QAbstractButton *>()) {
            if (candidate->accessibleName() == "Mute adult-provider playback on start") {
                mute = candidate;
                break;
            }
        }
        QVERIFY(mute);
        QVERIFY(mute->isChecked());
        mute->click();
        QVERIFY(!settings.value("providers/muteNsfwByDefault").toBool());
    }

    void adultProviderSwitchPersistsAndNotifiesImmediately() {
        QTemporaryDir temporary;
        QSettings settings(temporary.filePath("settings.ini"), QSettings::IniFormat);
        CloudStream::SettingsPane pane(&settings);
        pane.showSection("Providers");
        QSignalSpy changed(&pane, &CloudStream::SettingsPane::settingChanged);
        QAbstractButton *adultSwitch = nullptr;
        for (auto *candidate : pane.findChildren<QAbstractButton *>()) {
            if (candidate->accessibleName() == "Show adult providers") {
                adultSwitch = candidate;
                break;
            }
        }
        QVERIFY(adultSwitch);
        QVERIFY(!adultSwitch->isChecked());

        adultSwitch->click();

        QVERIFY(settings.value("providers/allowNsfw").toBool());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first().first().toString(), QString("providers/allowNsfw"));
    }
};

QTEST_MAIN(SettingsPaneTest)
#include "test_settings_pane.moc"
