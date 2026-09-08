QT += network concurrent
CS_VERSION = $$cat($$PWD/../VERSION, lines)
isEmpty(CS_VERSION): error(Missing CloudStream VERSION)
write_file($$OUT_PWD/cloudstream-version.txt, CS_VERSION)
DEFINES += CLOUDSTREAM_VERSION=\\\"$$CS_VERSION\\\"
SOURCES += $$PWD/ReleaseUpdater.cpp $$PWD/UpdatePane.cpp $$PWD/../network/CloudStreamRequest.cpp
HEADERS += $$PWD/ReleaseUpdater.h $$PWD/ReleasePolicy.h $$PWD/UpdatePane.h $$PWD/BuildInfo.h $$PWD/UpdateInstaller.h
