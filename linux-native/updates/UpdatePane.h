#pragma once
#include "ReleaseUpdater.h"
#include <QWidget>
#include <QFutureWatcher>
class QSettings; class QLabel; class QPushButton; class QProgressBar; class QPlainTextEdit;
namespace CloudStream::Updates {
class UpdatePane final : public QWidget {
 Q_OBJECT
public:
 UpdatePane(ReleaseUpdater *updater,QSettings *settings,QWidget *parent=nullptr);
 ~UpdatePane() override;
private:
 void refresh();
 void apply(bool manual=false);
 ReleaseUpdater *updater_;
 Package installed_;
 QLabel *versions_, *status_;
 QPlainTextEdit *notes_;
 QPushButton *check_, *download_, *cancel_, *apply_, *reveal_;
 QProgressBar *progress_;
 QFutureWatcher<QString> worker_;
 bool installing_=false;
};
}
