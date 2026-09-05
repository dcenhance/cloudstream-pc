#include "../media/ArtworkSizing.h"

#include <QtTest>

class ArtworkSizingTest : public QObject {
    Q_OBJECT

private slots:
    void usesStandardPosterAndBackdropRatios() {
        QCOMPARE(CloudStream::ArtworkSizing::posterSize(160), QSize(160, 240));
        QCOMPARE(CloudStream::ArtworkSizing::posterSize(150), QSize(150, 225));
        QCOMPARE(CloudStream::ArtworkSizing::backdropSize(240), QSize(240, 135));
    }

    void centerCropsWithoutDistortion() {
        QImage source(300, 100, QImage::Format_RGB32);
        source.fill(Qt::black);
        for (int x = 0; x < 100; ++x) for (int y = 0; y < 100; ++y) source.setPixelColor(x, y, Qt::red);
        for (int x = 100; x < 200; ++x) for (int y = 0; y < 100; ++y) source.setPixelColor(x, y, Qt::green);
        for (int x = 200; x < 300; ++x) for (int y = 0; y < 100; ++y) source.setPixelColor(x, y, Qt::blue);

        const auto cropped = CloudStream::ArtworkSizing::centerCrop(source, QSize(100, 100));
        QCOMPARE(cropped.size(), QSize(100, 100));
        QCOMPARE(cropped.pixelColor(50, 50), QColor(Qt::green));
    }
};

QTEST_APPLESS_MAIN(ArtworkSizingTest)
#include "test_artwork_sizing.moc"
