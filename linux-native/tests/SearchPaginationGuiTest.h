#pragma once
#include <QObject>
#include <QTemporaryDir>
class QWidget;
class SearchPaginationGuiTest final : public QObject {
    Q_OBJECT
    QTemporaryDir profile;
    static bool providerReady(QWidget &window);
private slots:
    void initTestCase();
    void loadsSecondPageOnlyWhenRequestedAndPreservesFilters();
    void staleSearchCannotAppendAfterNewQuery();
    void providerSelectionChangeInvalidatesPendingPage();
};
