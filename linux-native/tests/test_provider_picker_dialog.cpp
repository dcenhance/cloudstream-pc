#include <QtTest>

#include "../providers/ProviderPickerDialog.h"
#include "../providers/ProviderSelectionModel.h"

#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>

class ProviderPickerDialogTest final : public QObject {
    Q_OBJECT

    static QJsonObject provider(const QString &jar, const QString &name,
                                const QString &language, const QJsonArray &types) {
        return {
            {"mode", "provider"}, {"jarPath", jar}, {"name", name},
            {"language", language}, {"hasMainPage", true},
            {"mainUrl", "https://" + name.toLower() + ".example"},
            {"extensionName", name + "Extension"}, {"supportedTypes", types},
        };
    }

    static int visibleProviderRows(QListWidget *list) {
        int count = 0;
        for (int row = 0; row < list->count(); ++row) {
            auto *item = list->item(row);
            if (!item->isHidden() && item->data(Qt::UserRole).toString() == "provider") ++count;
        }
        return count;
    }

private slots:
    void embedsInsideTheApplicationSurface() {
        QWidget host;
        host.resize(1000, 800);
        host.show();
        CloudStream::ProviderPickerDialog picker({}, "none", {}, &host);
        picker.show();

        QVERIFY(!picker.isWindow());
        QCOMPARE(picker.parentWidget(), &host);
        QCOMPARE(picker.geometry(), host.rect());
        QVERIFY(picker.property("controllerNavigationScope").toBool());
        QVERIFY(picker.findChild<QWidget *>("providerPickerPanel"));
    }

    void buildsAndroidStyleSectionsAndRestoresSelection() {
        const auto ani = provider("ani.jar", "AniWorld", "de", {"Anime", "AnimeMovie"});
        const auto live = provider("live.jar", "Kool Live TV", "de", {"Live"});
        const auto selected = CloudStream::ProviderSelectionModel::key(ani);
        CloudStream::ProviderPickerDialog dialog({live, ani}, selected, {selected});

        auto *list = dialog.findChild<QListWidget *>("providerPickerList");
        QVERIFY(list);
        QCOMPARE(list->item(0)->text(), QString("QUICK CHOICES"));
        QCOMPARE(list->item(1)->data(Qt::UserRole).toString(), QString("none"));
        QCOMPARE(list->item(2)->data(Qt::UserRole).toString(), QString("random"));
        QCOMPARE(list->item(3)->text(), QString("PROVIDERS · 2"));
        QCOMPARE(list->item(4)->data(Qt::UserRole + 1).toString(), selected);
        QVERIFY(list->item(4)->data(Qt::UserRole + 4).toBool());
        QCOMPARE(list->currentItem(), list->item(4));
    }

    void filtersBySearchAndMediaTypeWithoutHidingQuickChoices() {
        const auto ani = provider("ani.jar", "AniWorld", "de", {"Anime"});
        const auto live = provider("live.jar", "Kool Live TV", "de", {"Live"});
        CloudStream::ProviderPickerDialog dialog({ani, live}, "none", {});
        dialog.show();

        auto *list = dialog.findChild<QListWidget *>("providerPickerList");
        auto *search = dialog.findChild<QLineEdit *>("providerPickerSearch");
        QVERIFY(list);
        QVERIFY(search);
        QCOMPARE(visibleProviderRows(list), 2);

        search->setText("kool live");
        QTRY_COMPARE(visibleProviderRows(list), 1);
        QVERIFY(!list->item(1)->isHidden());
        QVERIFY(!list->item(2)->isHidden());

        search->clear();
        auto *anime = dialog.findChild<QPushButton *>("providerTypeAnime");
        QVERIFY(anime);
        anime->click();
        QTRY_COMPARE(visibleProviderRows(list), 1);
    }

    void activatingAProviderReturnsItsStableKey() {
        const auto ani = provider("ani.jar", "AniWorld", "de", {"Anime"});
        const auto key = CloudStream::ProviderSelectionModel::key(ani);
        CloudStream::ProviderPickerDialog dialog({ani}, "none", {});
        auto *list = dialog.findChild<QListWidget *>("providerPickerList");
        QVERIFY(list);
        QSignalSpy accepted(&dialog, &CloudStream::ProviderPickerDialog::accepted);
        dialog.show();
        auto *row = list->itemWidget(list->item(4));
        QVERIFY(row);

        QTest::mouseClick(row, Qt::LeftButton, Qt::NoModifier, row->rect().center());
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(dialog.selectedKey(), key);
    }

    void visiblePinActionDoesNotCloseOrSelectTheProvider() {
        const auto ani = provider("ani.jar", "AniWorld", "de", {"Anime"});
        const auto key = CloudStream::ProviderSelectionModel::key(ani);
        CloudStream::ProviderPickerDialog dialog({ani}, "none", {});
        dialog.show();
        auto *pin = dialog.findChild<QToolButton *>("providerPickerPin");
        QVERIFY(pin);

        pin->click();

        QVERIFY(dialog.pinnedProviderKeys().contains(key));
        QCOMPARE(dialog.selectedKey(), QString("none"));
        QCOMPARE(dialog.result(), 0);
    }

    void hidesNsfwTypeChipWhenNoAdultProviderIsAllowed() {
        const auto safe = provider("safe.jar", "Safe", "en", {"Movie"});
        const auto adult = provider("adult.jar", "Adult", "en", {"NSFW"});
        CloudStream::ProviderPickerDialog hidden({safe}, "none", {});
        CloudStream::ProviderPickerDialog visible({safe, adult}, "none", {});
        auto *hiddenChip = hidden.findChild<QPushButton *>("providerTypeNsfw");
        auto *visibleChip = visible.findChild<QPushButton *>("providerTypeNsfw");
        QVERIFY(hiddenChip);
        QVERIFY(visibleChip);
        QVERIFY(hiddenChip->isHidden());
        QVERIFY(!visibleChip->isHidden());
    }
};

QTEST_MAIN(ProviderPickerDialogTest)
#include "test_provider_picker_dialog.moc"
