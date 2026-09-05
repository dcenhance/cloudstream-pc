QT += widgets network openglwidgets concurrent
CONFIG += c++17 link_pkgconfig
PKGCONFIG += mpv sdl2
TEMPLATE = app
TARGET = cloudstream-linux
VERSION = 0.1.0

win32 {
    TARGET = cloudstream
    CONFIG -= link_pkgconfig
    # Set MPV_LIB and SDL2_LIB to absolute .lib files in the Windows build environment.
    INCLUDEPATH += $$(MPV_INCLUDE) $$(SDL2_INCLUDE)
    LIBS += $$(MPV_LIB) $$(SDL2_LIB)
    DEFINES += CLOUDSTREAM_WINDOWS
}
SOURCES += main.cpp \
    app/Logger.cpp \
    details/DetailsPresentation.cpp \
    downloads/DownloadManager.cpp \
    downloads/DownloadQueueStore.cpp \
    episodes/EpisodeCatalog.cpp \
    extensions/ExtensionListFilter.cpp \
    extensions/ExtensionInstallBatch.cpp \
    extensions/ExtensionRegistry.cpp \
    history/WatchHistoryStore.cpp \
    input/GamepadNavigation.cpp \
    network/CloudStreamRequest.cpp \
    media/ArtworkLoader.cpp \
    media/ArtworkSizing.cpp \
    providers/HomeContentLimiter.cpp \
    providers/HomeHeroSelection.cpp \
    providers/ProviderConfiguration.cpp \
    providers/ProviderPickerDialog.cpp \
    providers/ProviderPreferenceFilter.cpp \
    providers/ProviderSelectionModel.cpp \
    providers/ProviderValidation.cpp \
    player/IntegratedPlayerWindow.cpp \
    player/MpvPlayerWidget.cpp \
    player/MpvIpcProtocol.cpp \
    player/PlayerCommand.cpp \
    player/SourceCatalog.cpp \
    repositories/RepositoryManifestParser.cpp \
    repositories/RepositoryUrlResolver.cpp \
    search/SearchHistoryModel.cpp \
    settings/SettingsPane.cpp \
    storage/XdgPaths.cpp \
    ui/SmoothScrollController.cpp
HEADERS += app/Logger.h \
    app/ProcessCompletion.h \
    app/ProviderHostCommand.h \
    details/DetailsPresentation.h \
    downloads/DownloadManager.h \
    downloads/ProcessSuspension.h \
    downloads/DownloadQueueStore.h \
    episodes/EpisodeCatalog.h \
    extensions/ExtensionListFilter.h \
    extensions/ExtensionInstallBatch.h \
    extensions/ExtensionRegistry.h \
    history/WatchHistoryStore.h \
    input/GamepadNavigation.h \
    network/CloudStreamRequest.h \
    media/ArtworkLoader.h \
    media/ArtworkSizing.h \
    providers/HomeContentLimiter.h \
    providers/HomeHeroSelection.h \
    providers/ProviderConfiguration.h \
    providers/ProviderPickerDialog.h \
    providers/ProviderPreferenceFilter.h \
    providers/ProviderDiscoveryGeneration.h \
    providers/ProviderSelectionModel.h \
    providers/ProviderValidation.h \
    player/IntegratedPlayerWindow.h \
    player/MpvPlayerWidget.h \
    player/MpvIpcProtocol.h \
    player/PlayerCommand.h \
    player/SourceCatalog.h \
    repositories/RepositoryManifestParser.h \
    repositories/RepositoryUrlResolver.h \
    search/SearchHistoryModel.h \
    settings/SettingsPane.h \
    storage/XdgPaths.h \
    ui/SmoothScrollController.h
RESOURCES += resources.qrc

isEmpty(PREFIX): PREFIX = /usr
target.path = $$PREFIX/bin

desktop.path = $$PREFIX/share/applications
desktop.files = $$PWD/packaging/io.github.recloudstream.cloudstream.desktop

icon.path = $$PREFIX/share/icons/hicolor/scalable/apps
icon.files = $$PWD/packaging/io.github.recloudstream.cloudstream.svg

metainfo.path = $$PREFIX/share/metainfo
metainfo.files = $$PWD/packaging/io.github.recloudstream.cloudstream.metainfo.xml

provider_host.path = $$PREFIX/libexec/cloudstream/provider-host
provider_host.files = \
    $$PWD/../provider-host/build/install/cloudstream-provider-host/bin \
    $$PWD/../provider-host/build/install/cloudstream-provider-host/lib

INSTALLS += target desktop icon metainfo provider_host
