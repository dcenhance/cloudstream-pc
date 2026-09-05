#include "../repositories/RepositoryUrlResolver.h"

#include <QtTest>

class RepositoryUrlResolverTest : public QObject {
    Q_OBJECT

private slots:
    void resolvesDirectUrl() {
        QCOMPARE(CloudStream::RepositoryUrlResolver::resolveShortForm(" https://example.org/repo.json "), QString("https://example.org/repo.json"));
    }

    void resolvesCloudStreamScheme() {
        QCOMPARE(CloudStream::RepositoryUrlResolver::resolveShortForm("cloudstreamrepo://example.org/repo.json"), QString("https://example.org/repo.json"));
    }

    void resolvesCsRepoQueryForm() {
        QCOMPARE(CloudStream::RepositoryUrlResolver::resolveShortForm("https://cs.repo/?example.org/repo.json"), QString("https://example.org/repo.json"));
    }

    void resolvesCsRepoPathForm() {
        QCOMPARE(CloudStream::RepositoryUrlResolver::resolveShortForm("cs.repo/example.org/repo.json"), QString("https://example.org/repo.json"));
    }

    void resolvesPyMdShortCode() {
        QCOMPARE(CloudStream::RepositoryUrlResolver::resolveShortForm("!abc_123"), QString("https://py.md/abc_123"));
    }

    void resolvesCuttlyShortCode() {
        QCOMPARE(CloudStream::RepositoryUrlResolver::resolveShortForm("abc-123"), QString("https://cutt.ly/abc-123"));
    }

    void convertsGithubRepositoryPage() {
        QCOMPARE(CloudStream::RepositoryUrlResolver::manifestUrl("https://github.com/recloudstream/extensions"), QString("https://raw.githubusercontent.com/recloudstream/extensions/master/repo.json"));
    }

    void rejectsUnsupportedInput() {
        QVERIFY(!CloudStream::RepositoryUrlResolver::isSupportedInput("file:///tmp/repo.json"));
        QVERIFY(!CloudStream::RepositoryUrlResolver::isSupportedInput("not a repo"));
        QVERIFY(CloudStream::RepositoryUrlResolver::isSupportedInput("https://example.org/repo.json"));
    }
};

QTEST_APPLESS_MAIN(RepositoryUrlResolverTest)
#include "test_repository_url_resolver.moc"
