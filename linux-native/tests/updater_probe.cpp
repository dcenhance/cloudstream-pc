// Explicit read-only / isolated-download diagnostic. Never installs or executes.
#include "../updates/ReleaseUpdater.h"
#include "../updates/BuildInfo.h"
#include "../updates/UpdateInstaller.h"
#include <QCoreApplication>
#include <QDebug>
using namespace CloudStream::Updates;
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv);
 qInstallMessageHandler([](QtMsgType,const QMessageLogContext &,const QString &text){ fprintf(stdout,"%s\n",qPrintable(text)); fflush(stdout); });
 const bool download=app.arguments().contains("--verify-download");
 qInfo() << (download?"Download probe: simulated old version 0.0.0, isolated temporary files only":"Live check with actual compiled version");
 ReleaseUpdater updater(download?QString("0.0.0"):buildVersion(),Package::AppImage);
 bool started=false;
 QObject::connect(&updater,&ReleaseUpdater::changed,&app,[&]{
  if(updater.busy()) return;
  qInfo().noquote()<<updater.status()<<"latest="<<updater.release().version;
  if(download && updater.available() && !started) {started=true;updater.download();return;}
  if(download) {
   bool ok=verifyPackage(updater.downloadedPath(),updater.release().sha256,updater.release().size);
   qInfo().noquote()<<"downloadVerified="<<ok<<"bytes="<<updater.release().size<<"sha256="<<updater.release().sha256.toHex()<<"temporaryPath="<<updater.downloadedPath();
   app.exit(ok?0:1);
  } else app.exit(updater.release().version.isEmpty()?1:0);
 });
 QTimer::singleShot(0,&updater,&ReleaseUpdater::check);
 return app.exec();
}
