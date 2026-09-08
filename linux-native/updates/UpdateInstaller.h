#pragma once
#include "ReleasePolicy.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QLockFile>
namespace CloudStream::Updates {
// Called on a worker thread. No commands, privilege elevation or profile writes.
inline bool verifyPackage(const QString &path,const QByteArray &digest,qint64 size) {
 QFileInfo info(path); if(info.isSymLink() || !info.isFile() || size<=0 || size>1024LL*1024*1024 || info.size()!=size || digest.size()!=32) return false;
 QFile file(path); if(!file.open(QIODevice::ReadOnly)) return false;
 QCryptographicHash hash(QCryptographicHash::Sha256); qint64 read=0;
 while(!file.atEnd()) { auto bytes=file.read(65536); if(bytes.isEmpty() || (read+=bytes.size())>size) return false; hash.addData(bytes); }
 return read==size && hash.result()==digest;
}
inline QString replaceAppImage(const QString &source,const QString &target,const QByteArray &digest,qint64 size) {
 const QFileInfo info(target), parent(info.absolutePath());
 if(!info.isAbsolute() || !info.isFile() || info.isSymLink() || info.canonicalFilePath()!=target || !target.endsWith(".AppImage") ||
    !info.isWritable() || !parent.isWritable() || target.contains("/.mount_") || target.startsWith("/proc/") || target.startsWith("/sys/") || target.startsWith("/dev/"))
  return "AppImage location is not a writable regular AppImage. Use the verified download for manual replacement.";
 QLockFile lock(target+".update-lock"); if(!lock.tryLock(0)) return "Another AppImage update is in progress.";
 if(QFileInfo::exists(target+".backup") || QFileInfo(target+".backup").isSymLink()) return "A rollback file already exists: "+target+".backup. Keep or move it before updating again.";
 if(!verifyPackage(source,digest,size)) return "Package changed or SHA-256 mismatch; installation unchanged.";
 QFile input(source); if(!input.open(QIODevice::ReadOnly)) return "Cannot read verified package.";
 auto signature=input.peek(12);
 if(!signature.startsWith(QByteArray::fromHex("7f454c46")) || signature.mid(8,3)!=QByteArray::fromHex("414902")) return "Verified package is not a type-2 AppImage.";
 QSaveFile replacement(target); replacement.setDirectWriteFallback(false);
 if(!replacement.open(QIODevice::WriteOnly)) return "Cannot stage replacement beside the AppImage.";
 if(!replacement.setPermissions(info.permissions()|QFile::ExeOwner)) return "Cannot preserve AppImage permissions.";
 QCryptographicHash hash(QCryptographicHash::Sha256); qint64 written=0;
 while(!input.atEnd()) {
  auto bytes=input.read(65536); if(bytes.isEmpty() || (written+=bytes.size())>size || replacement.write(bytes)!=bytes.size()) return "Cannot stage complete AppImage (check free disk space).";
  hash.addData(bytes);
 }
 if(written!=size || hash.result()!=digest) return "Package changed while staging; installation unchanged.";
 // Copy rollback before atomic rename. QFile::copy refuses any existing backup.
 if(!QFile::copy(target,target+".backup")) return "Cannot create rollback backup; installation unchanged.";
 if(!replacement.commit()) return "Atomic replacement failed. Original installation and backup retained.";
 return {};
}
}
