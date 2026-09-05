#include <QtTest>

#include "../ui/SmoothScrollController.h"

#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QWheelEvent>

class SmoothScrollControllerTest final : public QObject {
    Q_OBJECT

    static void wheel(QWidget *target, QPoint pixel, QPoint angle,
                      Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QWheelEvent event(QPointF(20, 20), QPointF(20, 20), pixel, angle,
                          Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QApplication::sendEvent(target, &event);
    }

private slots:
    void derivesAnimationCadenceFromDisplayRefreshRate() {
        QCOMPARE(CloudStream::SmoothScrollController::frameIntervalForRefreshRate(60.0), 17);
        QCOMPARE(CloudStream::SmoothScrollController::frameIntervalForRefreshRate(120.0), 8);
        QCOMPARE(CloudStream::SmoothScrollController::frameIntervalForRefreshRate(240.0), 4);
        QCOMPARE(CloudStream::SmoothScrollController::frameIntervalForRefreshRate(0.0), 16);
    }

    void easesVerticalMouseWheelAndAccumulatesRapidNotches() {
        QScrollArea area;
        auto *content = new QWidget;
        content->setFixedSize(500, 2400);
        area.setWidget(content);
        area.resize(500, 400);
        area.show();
        CloudStream::SmoothScrollController::attach(&area);
        QApplication::processEvents();

        wheel(area.viewport(), {}, QPoint(0, -120));
        wheel(area.viewport(), {}, QPoint(0, -120));
        QTRY_VERIFY_WITH_TIMEOUT(area.verticalScrollBar()->value() > 0, 150);
        QTRY_VERIFY_WITH_TIMEOUT(area.verticalScrollBar()->value() >= 250, 600);
    }

    void verticalMouseWheelOnShelfScrollsContainingPageNotShelf() {
        QScrollArea page;
        auto *content = new QWidget;
        content->setFixedSize(520, 1800);
        auto *shelf = new QListWidget(content);
        shelf->setGeometry(0, 120, 500, 190);
        shelf->setViewMode(QListView::IconMode);
        shelf->setFlow(QListView::LeftToRight);
        shelf->setWrapping(false);
        shelf->setGridSize(QSize(150, 180));
        for (int index = 0; index < 20; ++index) shelf->addItem(QString::number(index));
        page.setWidget(content);
        page.resize(520, 420);
        page.show();
        CloudStream::SmoothScrollController::attach(&page);
        CloudStream::SmoothScrollController::attach(
            shelf, CloudStream::SmoothScrollController::HorizontalWheel);
        QApplication::processEvents();

        wheel(shelf->viewport(), {}, QPoint(0, -120));
        QTRY_VERIFY_WITH_TIMEOUT(page.verticalScrollBar()->value() > 0, 500);
        QCOMPARE(shelf->horizontalScrollBar()->value(), 0);
    }

    void horizontalMouseWheelUsesHalfStrength() {
        QListWidget shelf;
        shelf.setViewMode(QListView::IconMode);
        shelf.setFlow(QListView::LeftToRight);
        shelf.setWrapping(false);
        shelf.setGridSize(QSize(150, 180));
        shelf.setFixedSize(500, 190);
        for (int index = 0; index < 20; ++index) shelf.addItem(QString::number(index));
        shelf.show();
        CloudStream::SmoothScrollController::attach(
            &shelf, CloudStream::SmoothScrollController::HorizontalWheel);
        QApplication::processEvents();

        const auto initialPosition = shelf.horizontalScrollBar()->value();
        wheel(shelf.viewport(), {}, QPoint(-120, 0));
        QTRY_COMPARE_WITH_TIMEOUT(shelf.horizontalScrollBar()->value(),
                                  initialPosition + 155, 1200);
    }

    void horizontalTouchpadPixelsUseHalfStrength() {
        QListWidget shelf;
        shelf.setViewMode(QListView::IconMode);
        shelf.setFlow(QListView::LeftToRight);
        shelf.setWrapping(false);
        shelf.setGridSize(QSize(150, 180));
        shelf.setFixedSize(500, 190);
        for (int index = 0; index < 20; ++index) shelf.addItem(QString::number(index));
        shelf.show();
        CloudStream::SmoothScrollController::attach(
            &shelf, CloudStream::SmoothScrollController::HorizontalWheel);
        QApplication::processEvents();

        wheel(shelf.viewport(), QPoint(-40, 0), {});
        QCOMPARE(shelf.horizontalScrollBar()->value(), 20);
    }

    void horizontalTouchpadSubpixelsAccumulateAtHalfStrength() {
        QListWidget shelf;
        shelf.setViewMode(QListView::IconMode);
        shelf.setFlow(QListView::LeftToRight);
        shelf.setWrapping(false);
        shelf.setGridSize(QSize(150, 180));
        shelf.setFixedSize(500, 190);
        for (int index = 0; index < 20; ++index) shelf.addItem(QString::number(index));
        shelf.show();
        CloudStream::SmoothScrollController::attach(
            &shelf, CloudStream::SmoothScrollController::HorizontalWheel);
        QApplication::processEvents();

        for (int event = 0; event < 40; ++event) {
            wheel(shelf.viewport(), QPoint(-1, 0), {});
        }
        QCOMPARE(shelf.horizontalScrollBar()->value(), 20);
    }

    void horizontalShelfIgnoresMinorVerticalTouchpadNoise() {
        QListWidget shelf;
        shelf.setViewMode(QListView::IconMode);
        shelf.setFlow(QListView::LeftToRight);
        shelf.setWrapping(false);
        shelf.setGridSize(QSize(150, 180));
        shelf.setFixedSize(500, 190);
        for (int index = 0; index < 20; ++index) shelf.addItem(QString::number(index));
        shelf.show();
        CloudStream::SmoothScrollController::attach(
            &shelf, CloudStream::SmoothScrollController::HorizontalWheel);
        QApplication::processEvents();

        wheel(shelf.viewport(), QPoint(-40, 1), {});
        QCOMPARE(shelf.horizontalScrollBar()->value(), 20);
    }

    void preservesPreciseTouchpadPixelDeltas() {
        QScrollArea area;
        auto *content = new QWidget;
        content->setFixedSize(500, 1600);
        area.setWidget(content);
        area.resize(500, 400);
        area.show();
        CloudStream::SmoothScrollController::attach(&area);
        QApplication::processEvents();

        wheel(area.viewport(), QPoint(0, -37), {});
        QCOMPARE(area.verticalScrollBar()->value(), 37);
    }

    void mouseWheelGlideIsLongAndFrameDense() {
        QScrollArea area;
        auto *content = new QWidget;
        content->setFixedSize(500, 2400);
        area.setWidget(content);
        area.resize(500, 400);
        area.show();
        CloudStream::SmoothScrollController::attach(&area);
        QApplication::processEvents();

        wheel(area.viewport(), {}, QPoint(0, -120));
        QSet<int> sampledValues;
        for (int sample = 0; sample < 12; ++sample) {
            QTest::qWait(20);
            sampledValues.insert(area.verticalScrollBar()->value());
        }
        const auto afterOldAnimationWouldFinish = area.verticalScrollBar()->value();
        QTest::qWait(100);
        QVERIFY2(area.verticalScrollBar()->value() > afterOldAnimationWouldFinish,
                 "Wheel motion stopped too abruptly instead of gliding");
        QVERIFY2(sampledValues.size() >= 9,
                 "Wheel motion did not produce enough intermediate frames");
    }

    void externalScrollbarResetCancelsActiveSpring() {
        QScrollArea area;
        auto *content = new QWidget;
        content->setFixedSize(500, 2400);
        area.setWidget(content);
        area.resize(500, 400);
        area.show();
        CloudStream::SmoothScrollController::attach(&area);
        QApplication::processEvents();

        wheel(area.viewport(), {}, QPoint(0, -120));
        QTRY_VERIFY_WITH_TIMEOUT(area.verticalScrollBar()->value() > 0, 150);
        area.verticalScrollBar()->setValue(0);
        QTest::qWait(900);

        QCOMPARE(area.verticalScrollBar()->value(), 0);
    }
};

QTEST_MAIN(SmoothScrollControllerTest)
#include "test_smooth_scroll_controller.moc"
