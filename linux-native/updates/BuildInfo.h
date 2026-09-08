#pragma once
#include "ReleasePolicy.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#ifndef CLOUDSTREAM_VERSION
#define CLOUDSTREAM_VERSION "0.0.0-dev"
#endif
namespace CloudStream::Updates {
inline QString buildVersion() { return QStringLiteral(CLOUDSTREAM_VERSION); }
inline QString packageLabel(Package p) {
 switch(p) {
 case Package::WindowsSetup: return "Windows installed (x64)";
 case Package::WindowsZip: return "Windows portable ZIP (x64)";
 case Package::AppImage: return "Linux AppImage (x86_64)";
 case Package::Deb: return "Linux DEB (amd64)";
 case Package::Rpm: return "Linux RPM (x86_64)";
 default: return "Source / development build";
 }
}
inline Package readPackageIdentity(const QString &version,const QString &package,const QString &arch) {
 if(version!=buildVersion() || !parseVersion(version) || arch!="x86_64") return Package::Source;
#ifdef Q_OS_WIN
 if(package=="windows-setup") return Package::WindowsSetup;
 if(package=="windows-zip") return Package::WindowsZip;
#elif defined(Q_OS_LINUX)
 if(package=="appimage") return Package::AppImage;
 if(package=="deb") return Package::Deb;
 if(package=="rpm") return Package::Rpm;
#endif
 return Package::Source;
}
inline Package installedPackage() {
 QFile f(QCoreApplication::applicationDirPath()+"/cloudstream-build.json");
 if(!f.open(QIODevice::ReadOnly) || f.size()>4096) return Package::Source;
 const auto o=QJsonDocument::fromJson(f.readAll()).object();
 return readPackageIdentity(o["version"].toString(),o["package"].toString(),QSysInfo::buildCpuArchitecture());
}
}
