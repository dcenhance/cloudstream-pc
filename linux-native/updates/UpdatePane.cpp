#include "UpdatePane.h"
#include "BuildInfo.h"
#include "UpdateInstaller.h"
#include <QCheckBox>
#include <QDesktopServices>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QProcess>
#include <QProcessEnvironment>
#include <QtConcurrent>
namespace CloudStream::Updates {
UpdatePane::UpdatePane(ReleaseUpdater *updater,QSettings *settings,QWidget *parent)
 : QWidget(parent),updater_(updater),installed_(installedPackage()) {
 setObjectName("appUpdateCard");
 setAttribute(Qt::WA_StyledBackground, true);
 setStyleSheet("QWidget#appUpdateCard{background:#15151b;border:1px solid #30303a;border-radius:12px;} QLabel{background:transparent;color:#c9c9d2;} QPushButton{background:#252530;color:#f3f3f5;border:1px solid #3c3c4b;border-radius:8px;padding:9px 14px;} QPushButton:hover{background:#343449;} QPushButton:focus{border:1px solid #8b9bff;} QPushButton:disabled{color:#70707e;} QPlainTextEdit{background:#101015;color:#bdbdc8;border:1px solid #30303a;border-radius:8px;padding:8px;} QCheckBox{color:#c9c9d2;background:transparent;spacing:9px;} QCheckBox::indicator{width:18px;height:18px;border:1px solid #73738a;border-radius:4px;background:#20202b;} QCheckBox::indicator:checked{background:#536dfe;image:url(:/icons/check.svg);} QCheckBox::indicator:focus{border:2px solid #9aacff;} QProgressBar{border:1px solid #3c3c4b;border-radius:5px;text-align:center;color:#eee;background:#101015;} QProgressBar::chunk{background:#536dfe;}");
 auto *layout=new QVBoxLayout(this); layout->setContentsMargins(18,16,18,16); layout->setSpacing(12);
 auto *heading=new QLabel("CloudStream PC updates"); heading->setStyleSheet("font-size:18px;font-weight:700;color:#f3f3f5;"); layout->addWidget(heading);
 versions_=new QLabel; versions_->setObjectName("appUpdateVersions"); versions_->setTextFormat(Qt::PlainText); versions_->setWordWrap(true); layout->addWidget(versions_);
 auto *origin=new QLabel("Official releases · github.com/dcenhance/cloudstream-pc\n"+packageLabel(installed_)); origin->setWordWrap(true); layout->addWidget(origin);
 if(installed_==Package::Source || installed_==Package::WindowsZip || installed_==Package::Deb || installed_==Package::Rpm) {
  auto *limit=new QLabel(installed_==Package::Source ? "Development build: verified package download only; this running build will not install or execute it." : installed_==Package::WindowsZip ? "Portable ZIP: extract into a new folder after closing CloudStream. Your profile is kept separately. No running files are overwritten." : "DEB / RPM: download and verify here, then install with your distribution’s package manager. System authorization is handled by your desktop, never by CloudStream.");
  limit->setWordWrap(true); layout->addWidget(limit);
 }
 auto *automatic=new QCheckBox("Check automatically on startup (never download or install)"); automatic->setObjectName("automaticAppUpdates"); automatic->setChecked(settings->value("updates/automaticCheck",false).toBool());
 connect(automatic,&QCheckBox::toggled,this,[settings](bool on){settings->setValue("updates/automaticCheck",on);settings->sync();}); layout->addWidget(automatic);
 status_=new QLabel; status_->setObjectName("appUpdateStatus"); status_->setTextFormat(Qt::PlainText); status_->setWordWrap(true); layout->addWidget(status_);
 notes_=new QPlainTextEdit; notes_->setObjectName("appUpdateNotes"); notes_->setReadOnly(true); notes_->setAccessibleName("Release notes"); notes_->setPlaceholderText("Release notes will appear here after checking."); notes_->setMinimumHeight(120); notes_->setMaximumHeight(210); layout->addWidget(notes_);
 progress_=new QProgressBar; progress_->setObjectName("appUpdateProgress"); progress_->setRange(0,100); progress_->setValue(0); layout->addWidget(progress_);
 auto *buttons=new QHBoxLayout; layout->addLayout(buttons);
 check_=new QPushButton("Check for updates"); check_->setObjectName("checkAppUpdates"); buttons->addWidget(check_);
 download_=new QPushButton("Download update"); download_->setObjectName("downloadAppUpdate"); buttons->addWidget(download_);
 cancel_=new QPushButton("Cancel"); cancel_->setObjectName("cancelAppUpdate"); buttons->addWidget(cancel_);
 apply_=new QPushButton(installed_==Package::WindowsSetup ? "Run verified installer…" : installed_==Package::AppImage ? "Replace AppImage…" : "Show verified package…"); apply_->setObjectName("applyAppUpdate"); layout->addWidget(apply_);
 reveal_=new QPushButton("Show verified package for manual installation…"); reveal_->setObjectName("showAppUpdatePackage");
 reveal_->setVisible(installed_==Package::WindowsSetup || installed_==Package::AppImage); layout->addWidget(reveal_);
 connect(reveal_,&QPushButton::clicked,this,[this]{apply(true);});
 connect(check_,&QPushButton::clicked,updater_,&ReleaseUpdater::check);
 connect(cancel_,&QPushButton::clicked,updater_,&ReleaseUpdater::cancel);
 connect(download_,&QPushButton::clicked,this,[this]{
  if(QMessageBox::question(this,"Download update",QString("Download %1 from the official GitHub release?\nSHA-256 is checked before the package is made available. Nothing will be installed yet.").arg(updater_->release().name),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes) updater_->download();
 });
 connect(apply_,&QPushButton::clicked,this,[this]{apply(false);});
 connect(updater_,&ReleaseUpdater::changed,this,&UpdatePane::refresh);
 connect(updater_,&ReleaseUpdater::progress,this,[this](qint64 received,qint64 total){progress_->setRange(0,100); progress_->setValue(int(received*100/qMax(qint64(1),total))); progress_->setFormat(QString("%1 / %2 MiB — %p%").arg(received/1048576).arg(total/1048576));});
 refresh();
}
UpdatePane::~UpdatePane() { worker_.waitForFinished(); }
void UpdatePane::refresh() {
 if(installing_) return;
 const bool busy=updater_->busy();
 versions_->setText("Current: "+updater_->currentVersion()+"    Latest compatible: "+(updater_->release().version.isEmpty()?QString("not checked / unavailable"):updater_->release().version));
 status_->setText(updater_->status()+(updater_->downloadedPath().isEmpty()?QString():QString("\n")+updater_->downloadedPath())); notes_->setPlainText(updater_->release().notes);
 check_->setEnabled(!busy); download_->setEnabled(!busy && updater_->available()); cancel_->setEnabled(busy); apply_->setEnabled(!busy && !updater_->downloadedPath().isEmpty()); reveal_->setEnabled(apply_->isEnabled());
 if(busy && updater_->status().startsWith("Checking")) {progress_->setRange(0,0);progress_->setFormat("Checking…");}
 else if(!busy) {progress_->setRange(0,100);progress_->setValue(updater_->downloadedPath().isEmpty()?0:100);progress_->setFormat("%p%");}
}
void UpdatePane::apply(bool manual) {
 if(installing_ || updater_->busy() || updater_->downloadedPath().isEmpty()) return;
 const QString source=updater_->downloadedPath(); const auto release=updater_->release();
 const bool appimage=!manual && installed_==Package::AppImage && updater_->package()==Package::AppImage;
 const bool setup=!manual && installed_==Package::WindowsSetup && updater_->package()==Package::WindowsSetup;
 QString target;
 if(appimage) {
  target=qEnvironmentVariable("APPIMAGE"); const auto appdir=QFileInfo(qEnvironmentVariable("APPDIR")).canonicalFilePath();
  if(target.isEmpty() || appdir.isEmpty() || !QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath().startsWith(appdir+"/")) {
   QMessageBox::warning(this,"AppImage update","Cannot identify the running AppImage. Launch the original AppImage, not a copied executable or extracted mount."); return;
  }
 }
#ifdef Q_OS_WIN
 if(setup) {
  const auto expected=QFileInfo(qEnvironmentVariable("LOCALAPPDATA")+"/Programs/CloudStream PC").canonicalFilePath();
  if(expected.isEmpty() || expected.compare(QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath(),Qt::CaseInsensitive)!=0) {
   QMessageBox::warning(this,"Installer location","This installation is not in the supported per-user installer directory. Download the Setup package and install manually to preserve your chosen location."); return;
  }
 }
#endif
 const auto text=appimage ? "Replace this AppImage atomically?\n"+target+"\nThe previous file is preserved as .backup. Your profile is untouched. Restart is a separate choice." : setup ? QString("Run the verified official Setup installer and close CloudStream?\nThe interactive per-user installer preserves your profile. No silent install or elevation is requested.") : QString("Keep and show the verified package for manual installation?\n"+source+"\nCloudStream will not execute or unpack it. Close the app before replacing portable files; use your system package manager for DEB/RPM.");
 if(QMessageBox::question(this,"Confirm update",text,QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
 updater_->retainDownload(); installing_=true;
 check_->setEnabled(false); download_->setEnabled(false); cancel_->setEnabled(false); apply_->setEnabled(false); reveal_->setEnabled(false);
 status_->setText(appimage?"Re-verifying and staging replacement; do not close the app…":"Re-verifying package before handoff…"); progress_->setRange(0,0);
 disconnect(&worker_,nullptr,this,nullptr);
 connect(&worker_,&QFutureWatcher<QString>::finished,this,[this,appimage,setup,target,source]{
  installing_=false; const auto error=worker_.result(); refresh();
  if(!error.isEmpty()) {status_->setText(error);return;}
  if(appimage) {
   apply_->setEnabled(false); download_->setEnabled(false); status_->setText("AppImage replaced. Previous file: "+target+".backup. Restart to use the new version.");
   if(QMessageBox::question(this,"AppImage ready","Restart CloudStream now? Your previous AppImage remains as .backup; rename it back if you need to roll back.",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes) {
    QProcess process; auto env=QProcessEnvironment::systemEnvironment(); env.remove("APPIMAGE");env.remove("APPDIR");env.remove("OWD"); process.setProcessEnvironment(env);process.setProgram(target);process.setArguments({});process.setWorkingDirectory(QFileInfo(target).absolutePath());
    if(process.startDetached()) QCoreApplication::quit(); else status_->setText("AppImage replaced, but restart failed. Start it manually: "+target);
   }
  } else if(setup) {
#ifdef Q_OS_WIN
   if(QProcess::startDetached(source,QStringList{},QFileInfo(source).absolutePath())) QCoreApplication::quit();
   else status_->setText("Installer could not start. Verified package retained: "+source);
#endif
  } else {
   const bool opened=QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(source).absolutePath()));
   status_->setText((opened?"Verified package shown. Manual installation is still required: ":"Could not open the folder. Verified package retained for manual installation: ")+source);
  }
 });
 worker_.setFuture(QtConcurrent::run([source,target,release,appimage,setup]{
  if(appimage) return replaceAppImage(source,target,release.sha256,release.size);
  if(!verifyPackage(source,release.sha256,release.size)) return QString("Package changed or SHA-256 mismatch. Handoff blocked.");
  if(setup) {QFile f(source); if(!f.open(QIODevice::ReadOnly) || f.read(2)!="MZ") return QString("Verified package is not a Windows executable.");}
  return QString();
 }));
}
}
