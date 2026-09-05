#include "MpvPlayerWidget.h"

#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <algorithm>
#include <clocale>
#include <cstring>

#include <mpv/client.h>
#include <mpv/render_gl.h>

namespace CloudStream {
namespace {
QString mpvError(int code) {
    return QString::fromUtf8(mpv_error_string(code));
}

const mpv_node *mapValue(const mpv_node &node, const char *key) {
    if (node.format != MPV_FORMAT_NODE_MAP || !node.u.list) return nullptr;
    for (int index = 0; index < node.u.list->num; ++index) {
        if (node.u.list->keys[index] && std::strcmp(node.u.list->keys[index], key) == 0) {
            return &node.u.list->values[index];
        }
    }
    return nullptr;
}

QString nodeString(const mpv_node *node) {
    return node && node->format == MPV_FORMAT_STRING && node->u.string
        ? QString::fromUtf8(node->u.string) : QString();
}

int nodeInteger(const mpv_node *node, int fallback = -1) {
    return node && node->format == MPV_FORMAT_INT64 ? static_cast<int>(node->u.int64) : fallback;
}

bool nodeFlag(const mpv_node *node) {
    return node && node->format == MPV_FORMAT_FLAG && node->u.flag != 0;
}

QList<MpvTrack> parseTracks(const mpv_node *node) {
    QList<MpvTrack> tracks;
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) return tracks;
    for (int index = 0; index < node->u.list->num; ++index) {
        const auto &value = node->u.list->values[index];
        if (value.format != MPV_FORMAT_NODE_MAP) continue;
        MpvTrack track;
        track.id = nodeInteger(mapValue(value, "id"));
        track.type = nodeString(mapValue(value, "type"));
        track.title = nodeString(mapValue(value, "title"));
        track.language = nodeString(mapValue(value, "lang"));
        track.codec = nodeString(mapValue(value, "codec"));
        track.selected = nodeFlag(mapValue(value, "selected"));
        track.external = nodeFlag(mapValue(value, "external"));
        if (track.id >= 0 && !track.type.isEmpty()) tracks << track;
    }
    return tracks;
}

QString playbackUrl(const PlaybackSource &source) {
    if (source.playlist.isEmpty()) return source.url;
    QByteArray edl("edl://");
    for (int index = 0; index < source.playlist.size(); ++index) {
        if (index > 0) edl += ';';
        const auto url = source.playlist[index].url.toUtf8();
        edl += '%' + QByteArray::number(url.size()) + '%' + url;
        if (source.playlist[index].durationUs > 0) {
            edl += ",0," + QByteArray::number(
                source.playlist[index].durationUs / 1'000'000.0, 'f', 6);
        }
    }
    return QString::fromUtf8(edl);
}
} // namespace

MpvPlayerWidget::MpvPlayerWidget(QWidget *parent) : QOpenGLWidget(parent) {
    setMinimumSize(480, 270);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    std::setlocale(LC_NUMERIC, "C");
    handle = mpv_create();
    if (!handle) {
        errorText = "Could not create libmpv";
        return;
    }
    mpv_set_option_string(handle, "vo", "libmpv");
    mpv_set_option_string(handle, "hwdec", "auto-safe");
    mpv_set_option_string(handle, "keep-open", "yes");
    mpv_set_option_string(handle, "terminal", "no");
    mpv_set_option_string(handle, "input-default-bindings", "no");
    const auto initialized = mpv_initialize(handle);
    if (initialized < 0) {
        errorText = "Could not initialize libmpv: " + mpvError(initialized);
        mpv_terminate_destroy(handle);
        handle = nullptr;
        return;
    }
    available = true;
    mpv_request_log_messages(handle, "warn");
    mpv_set_wakeup_callback(handle, wakeup, this);
    mpv_observe_property(handle, PositionProperty, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle, DurationProperty, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle, PauseProperty, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(handle, VolumeProperty, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle, MuteProperty, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(handle, TrackListProperty, "track-list", MPV_FORMAT_NODE);
    setVolume(currentVolume);
}

MpvPlayerWidget::~MpvPlayerWidget() {
    if (handle) mpv_set_wakeup_callback(handle, nullptr, nullptr);
    if (renderContext) {
        makeCurrent();
        mpv_render_context_set_update_callback(renderContext, nullptr, nullptr);
        mpv_render_context_free(renderContext);
        renderContext = nullptr;
        doneCurrent();
    }
    if (handle) {
        mpv_terminate_destroy(handle);
        handle = nullptr;
    }
}

bool MpvPlayerWidget::isAvailable() const { return available; }
QString MpvPlayerWidget::initializationError() const { return errorText; }
double MpvPlayerWidget::position() const { return currentPosition; }
double MpvPlayerWidget::duration() const { return currentDuration; }
bool MpvPlayerWidget::isPaused() const { return paused; }
bool MpvPlayerWidget::isMuted() const { return currentMuted; }
int MpvPlayerWidget::volume() const { return currentVolume; }
double MpvPlayerWidget::playbackSpeed() const { return currentPlaybackSpeed; }
bool MpvPlayerWidget::fillMode() const { return currentFillMode; }
int MpvPlayerWidget::renderedFrameCount() const { return frameCount; }
QString MpvPlayerWidget::currentVideoOutput() const {
    if (!handle) return {};
    auto *value = mpv_get_property_string(handle, "current-vo");
    const auto result = value ? QString::fromUtf8(value) : QString();
    mpv_free(value);
    return result;
}
QList<MpvTrack> MpvPlayerWidget::tracks() const { return currentTracks; }

void MpvPlayerWidget::loadSource(const PlaybackSource &source,
                                 const QList<PlaybackSubtitle> &subtitles,
                                 double resumePositionSeconds) {
    pendingSource = source;
    pendingSubtitles = subtitles;
    resumePosition = std::max(0.0, resumePositionSeconds);
    hasPendingSource = true;
    externalAudioAdded = false;
    currentPosition = 0.0;
    currentDuration = 0.0;
    emit loadingChanged(true);
    if (renderContext) executePendingLoad();
}

void MpvPlayerWidget::setPaused(bool value) {
    if (!handle) return;
    int flag = value ? 1 : 0;
    mpv_set_property(handle, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvPlayerWidget::togglePaused() { setPaused(!paused); }

void MpvPlayerWidget::seekTo(double seconds) {
    if (!handle) return;
    command({"seek", QString::number(std::max(0.0, seconds), 'f', 3), "absolute", "exact"});
}

void MpvPlayerWidget::seekBy(double seconds) {
    if (!handle) return;
    command({"seek", QString::number(seconds, 'f', 3), "relative", "exact"});
}

void MpvPlayerWidget::setVolume(int value) {
    if (!handle) return;
    double normalized = std::clamp(static_cast<double>(value), 0.0, 100.0);
    mpv_set_property(handle, "volume", MPV_FORMAT_DOUBLE, &normalized);
}

void MpvPlayerWidget::setMuted(bool muted) {
    if (!handle) return;
    int flag = muted ? 1 : 0;
    mpv_set_property(handle, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvPlayerWidget::setPlaybackSpeed(double speed) {
    currentPlaybackSpeed = std::clamp(speed, 0.25, 4.0);
    if (!handle) return;
    mpv_set_property(handle, "speed", MPV_FORMAT_DOUBLE, &currentPlaybackSpeed);
}

void MpvPlayerWidget::setFillMode(bool fill) {
    currentFillMode = fill;
    if (!handle) return;
    double panscan = fill ? 1.0 : 0.0;
    mpv_set_property(handle, "panscan", MPV_FORMAT_DOUBLE, &panscan);
}

void MpvPlayerWidget::selectSubtitle(int index) {
    if (!handle) return;
    if (index < 0 || index >= pendingSubtitles.size()) {
        mpv_set_property_string(handle, "sid", "no");
        return;
    }
    const auto &subtitle = pendingSubtitles[index];
    command({"sub-add", subtitle.url, "select", subtitle.language});
}

void MpvPlayerWidget::setAudioTrack(int id) {
    if (!handle) return;
    if (id < 0) {
        mpv_set_property_string(handle, "aid", "auto");
        return;
    }
    auto value = static_cast<int64_t>(id);
    mpv_set_property(handle, "aid", MPV_FORMAT_INT64, &value);
}

void MpvPlayerWidget::setSubtitleTrack(int id) {
    if (!handle) return;
    if (id < 0) {
        mpv_set_property_string(handle, "sid", "no");
        return;
    }
    auto value = static_cast<int64_t>(id);
    mpv_set_property(handle, "sid", MPV_FORMAT_INT64, &value);
}

void MpvPlayerWidget::initializeGL() {
    if (!handle) return;
    mpv_opengl_init_params initialization{getProcAddress, nullptr};
    mpv_render_param parameters[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &initialization},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    const auto result = mpv_render_context_create(&renderContext, handle, parameters);
    if (result < 0) {
        errorText = "Could not create the libmpv OpenGL renderer: " + mpvError(result);
        available = false;
        QTimer::singleShot(0, this, [this] { emit playbackError(errorText); });
        return;
    }
    mpv_render_context_set_update_callback(renderContext, renderUpdate, this);
    executePendingLoad();
}

void MpvPlayerWidget::paintGL() {
    if (!renderContext) {
        if (auto *functions = context()->functions()) {
            functions->glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
            functions->glClear(GL_COLOR_BUFFER_BIT);
        }
        return;
    }
    const auto scale = devicePixelRatioF();
    mpv_opengl_fbo framebuffer{
        static_cast<int>(defaultFramebufferObject()),
        static_cast<int>(width() * scale),
        static_cast<int>(height() * scale),
        0,
    };
    int flip = 1;
    mpv_render_param parameters[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &framebuffer},
        {MPV_RENDER_PARAM_FLIP_Y, &flip},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    mpv_render_context_render(renderContext, parameters);
    mpv_render_context_report_swap(renderContext);
    ++frameCount;
}

void *MpvPlayerWidget::getProcAddress(void *, const char *name) {
    const auto function = QOpenGLContext::currentContext()->getProcAddress(QByteArray(name));
    return reinterpret_cast<void *>(function);
}

void MpvPlayerWidget::wakeup(void *context) {
    auto *player = static_cast<MpvPlayerWidget *>(context);
    QMetaObject::invokeMethod(player, [player] { player->processEvents(); }, Qt::QueuedConnection);
}

void MpvPlayerWidget::renderUpdate(void *context) {
    auto *player = static_cast<MpvPlayerWidget *>(context);
    QMetaObject::invokeMethod(player, [player] { player->update(); }, Qt::QueuedConnection);
}

void MpvPlayerWidget::processEvents() {
    if (!handle) return;
    while (true) {
        auto *event = mpv_wait_event(handle, 0.0);
        if (!event || event->event_id == MPV_EVENT_NONE) break;
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto *property = static_cast<mpv_event_property *>(event->data);
            if (!property || !property->data) continue;
            if (event->reply_userdata == PositionProperty && property->format == MPV_FORMAT_DOUBLE) {
                currentPosition = *static_cast<double *>(property->data);
                emit positionChanged(currentPosition);
            } else if (event->reply_userdata == DurationProperty && property->format == MPV_FORMAT_DOUBLE) {
                currentDuration = *static_cast<double *>(property->data);
                emit durationChanged(currentDuration);
            } else if (event->reply_userdata == PauseProperty && property->format == MPV_FORMAT_FLAG) {
                paused = *static_cast<int *>(property->data) != 0;
                emit pausedChanged(paused);
            } else if (event->reply_userdata == VolumeProperty && property->format == MPV_FORMAT_DOUBLE) {
                currentVolume = std::clamp(static_cast<int>(*static_cast<double *>(property->data)), 0, 100);
                emit volumeChanged(currentVolume);
            } else if (event->reply_userdata == MuteProperty && property->format == MPV_FORMAT_FLAG) {
                currentMuted = *static_cast<int *>(property->data) != 0;
                emit mutedChanged(currentMuted);
            } else if (event->reply_userdata == TrackListProperty && property->format == MPV_FORMAT_NODE) {
                currentTracks = parseTracks(static_cast<mpv_node *>(property->data));
                emit tracksChanged(currentTracks);
            }
        } else if (event->event_id == MPV_EVENT_START_FILE) {
            emit loadingChanged(true);
        } else if (event->event_id == MPV_EVENT_FILE_LOADED) {
            if (!externalAudioAdded) {
                externalAudioAdded = true;
                for (int index = 0; index < pendingSource.audioTracks.size(); ++index) {
                    const auto &audio = pendingSource.audioTracks[index];
                    command({"audio-add", audio.url, index == 0 ? "select" : "auto",
                             "External audio " + QString::number(index + 1)});
                }
            }
            addSubtitles();
            if (resumePosition > 0.0) seekTo(resumePosition);
            resumePosition = 0.0;
            emit loadingChanged(false);
            emit fileLoaded();
        } else if (event->event_id == MPV_EVENT_END_FILE) {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end && end->reason == MPV_END_FILE_REASON_ERROR) {
                emit playbackError(mpvError(end->error));
            } else if (end && end->reason == MPV_END_FILE_REASON_EOF) {
                emit endReached();
            }
        } else if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
            const auto *message = static_cast<mpv_event_log_message *>(event->data);
            if (message && message->text) qWarning().noquote() << "libmpv:" << QString::fromUtf8(message->text).trimmed();
        }
    }
}

void MpvPlayerWidget::executePendingLoad() {
    if (!handle || !renderContext || !hasPendingSource) return;
    hasPendingSource = false;
    QStringList fields;
    auto headers = pendingSource.httpHeaders();
    const auto primaryUrl = pendingSource.playlist.isEmpty()
        ? pendingSource.url : pendingSource.playlist.constFirst().url;
    const auto videoHost = QUrl(primaryUrl).host();
    for (const auto &audio : pendingSource.audioTracks) {
        if (videoHost.isEmpty() || QUrl(audio.url).host() != videoHost) continue;
        for (auto iterator = audio.headers.cbegin(); iterator != audio.headers.cend(); ++iterator) {
            if (!headers.contains(iterator.key())) headers.insert(iterator.key(), iterator.value());
        }
    }
    for (auto iterator = headers.cbegin(); iterator != headers.cend(); ++iterator) {
        fields << iterator.key() + ": " + iterator.value();
    }
    const auto encodedFields = fields.join(",").toUtf8();
    mpv_set_property_string(handle, "http-header-fields", encodedFields.constData());
    const auto encodedReferer = pendingSource.referer.toUtf8();
    mpv_set_property_string(handle, "referrer", encodedReferer.constData());
    command({"loadfile", playbackUrl(pendingSource), "replace"});
}

void MpvPlayerWidget::command(const QStringList &arguments) {
    if (!handle || arguments.isEmpty()) return;
    QVector<QByteArray> encoded;
    encoded.reserve(arguments.size());
    for (const auto &argument : arguments) encoded << argument.toUtf8();
    QVector<const char *> pointers;
    pointers.reserve(encoded.size() + 1);
    for (const auto &argument : encoded) pointers << argument.constData();
    pointers << nullptr;
    const auto result = mpv_command_async(handle, 0, pointers.data());
    if (result < 0) emit playbackError(mpvError(result));
}

void MpvPlayerWidget::addSubtitles() {
    for (const auto &subtitle : pendingSubtitles) command({"sub-add", subtitle.url, "auto", subtitle.language});
}

} // namespace CloudStream
