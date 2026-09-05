#include "../details/DetailsPresentation.h"

#include <QtTest>

class DetailsPresentationTest : public QObject {
    Q_OBJECT

private slots:
    void prefersBackdropAndFallsBackToPoster() {
        QCOMPARE(CloudStream::DetailsPresentation::backdropUrl({
            {"backgroundPosterUrl", "https://example.org/backdrop.jpg"},
            {"posterUrl", "https://example.org/poster.jpg"},
        }), QString("https://example.org/backdrop.jpg"));
        QCOMPARE(CloudStream::DetailsPresentation::backdropUrl({
            {"posterUrl", "https://example.org/poster.jpg"},
        }), QString("https://example.org/poster.jpg"));
    }

    void buildsReadableFactsFromProviderMetadata() {
        const QJsonObject details{
            {"year", 2025}, {"type", "Movie"}, {"duration", 121},
            {"contentRating", "PG-13"},
        };
        QCOMPARE(CloudStream::DetailsPresentation::facts(details, "Aniworld"),
                 QStringList({"2025", "Movie", "121 min", "PG-13", "Aniworld"}));
    }
};

QTEST_MAIN(DetailsPresentationTest)
#include "test_details_presentation.moc"
