#pragma once
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QUrl>
#include <optional>
namespace CloudStream::Updates {
enum class Package { Source, WindowsSetup, WindowsZip, AppImage, Deb, Rpm };
inline QString assetName(const QString &v, Package p) {
 const QString prefix="CloudStream-PC-"+v;
 switch(p) {
 case Package::WindowsSetup: return prefix+"-Windows-x64-Setup.exe";
 case Package::WindowsZip: return prefix+"-Windows-x64.zip";
 case Package::AppImage: return prefix+"-x86_64-system-runtime.AppImage";
 case Package::Deb: { auto deb=v; deb.replace('-', '.'); return "cloudstream-pc_"+deb+"_amd64.deb"; }
 case Package::Rpm: { auto parts=v.split('-'); auto release=parts.size()==1?QString("1"):QString("0.")+parts.mid(1).join('-'); return "cloudstream-pc-"+parts[0]+"-"+release+".x86_64.rpm"; }
 default: return {};
 }
}
inline bool trustedUrl(const QUrl &u, bool redirect = false) {
 if(!u.isValid() || u.scheme()!="https" || !u.userInfo().isEmpty() || (u.port()!=-1 && u.port()!=443) || u.hasFragment()) return false;
 if(u.host()=="api.github.com") return u.path()=="/repos/dcenhance/cloudstream-pc/releases/latest" && !u.hasQuery();
 if(u.host()=="github.com") return u.path().startsWith("/dcenhance/cloudstream-pc/releases/download/") && !u.hasQuery();
 return redirect && (u.host()=="release-assets.githubusercontent.com" || u.host()=="objects.githubusercontent.com") && u.path().startsWith("/github-production-release-asset/");
}
struct Version { QString text; QList<quint64> core; QStringList pre; };
inline std::optional<Version> parseVersion(QString text) {
 static const QRegularExpression re(QStringLiteral("\\Av?(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*))?(?:\\+([0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*))?\\z"));
 if (text.size() > 128) return {};
 auto m = re.match(text); if (!m.hasMatch()) return {};
 Version v; v.text = text.startsWith('v') ? text.mid(1) : text;
 for (int i=1;i<=3;++i) { bool ok=false; auto n=m.captured(i).toULongLong(&ok); if(!ok) return {}; v.core.append(n); }
 if (!m.captured(4).isEmpty()) v.pre=m.captured(4).split('.');
 for (const auto &p : v.pre) {
  bool numeric=true; for(auto c:p) if(c<'0'||c>'9') numeric=false;
  if(numeric && p.size()>1 && p.startsWith('0')) return {};
 }
 return v;
}
inline std::optional<int> compareVersions(const QString &left, const QString &right) {
 auto a=parseVersion(left), b=parseVersion(right); if(!a||!b) return {};
 for(int i=0;i<3;++i) if(a->core[i]!=b->core[i]) return a->core[i]>b->core[i]?1:-1;
 if(a->pre.isEmpty()!=b->pre.isEmpty()) return a->pre.isEmpty()?1:-1;
 static const QRegularExpression numeric("\\A[0-9]+\\z");
 for(int i=0;i<qMin(a->pre.size(),b->pre.size());++i) {
  auto x=a->pre[i], y=b->pre[i]; if(x==y) continue;
  bool nx=numeric.match(x).hasMatch(), ny=numeric.match(y).hasMatch();
  if(nx!=ny) return nx?-1:1;
  if(nx && x.size()!=y.size()) return x.size()>y.size()?1:-1;
  return x>y?1:-1;
 }
 return a->pre.size()==b->pre.size()?0:(a->pre.size()>b->pre.size()?1:-1);
}
}
