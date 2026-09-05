#include "GamepadNavigation.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QScrollArea>
#include <QTabBar>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>
#include <SDL.h>
#include <limits>

namespace CloudStream {
namespace {
constexpr int axisDeadZone = 16000;
constexpr qint64 initialRepeatDelayMs = 340;
constexpr qint64 repeatIntervalMs = 85;
constexpr Uint32 sdlSubsystemFlags =
    SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS;
int sdlGlobalOwners = 0;
QByteArray previousBackgroundHint;
bool previousBackgroundHintWasSet = false;
int previousControllerEventState = SDL_DISABLE;

quint64 controlKey(qint32 instanceId, int control) {
    return (quint64(quint32(instanceId)) << 32U) | quint32(control);
}

qint32 instanceFromControlKey(quint64 key) {
    return qint32(quint32(key >> 32U));
}

void acquireSdlGlobalState() {
    if (sdlGlobalOwners++ != 0) return;
    const char *hint = SDL_GetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);
    previousBackgroundHintWasSet = hint != nullptr;
    previousBackgroundHint = hint ? QByteArray(hint) : QByteArray();
    previousControllerEventState = SDL_GameControllerEventState(SDL_QUERY);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_GameControllerEventState(SDL_ENABLE);
}

void releaseSdlGlobalState() {
    if (sdlGlobalOwners <= 0 || --sdlGlobalOwners != 0) return;
    SDL_GameControllerEventState(previousControllerEventState);
    if (previousBackgroundHintWasSet) {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
                    previousBackgroundHint.constData());
    } else {
        SDL_ResetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);
    }
    previousBackgroundHint.clear();
    previousBackgroundHintWasSet = false;
}

QWidget *navigationScope(QWidget *origin) {
    if (!origin) return nullptr;
    if (auto *popup = QApplication::activePopupWidget()) return popup;
    if (auto *modal = QApplication::activeModalWidget()) return modal;
    for (auto *ancestor = origin; ancestor; ancestor = ancestor->parentWidget()) {
        if (ancestor->property("controllerNavigationScope").toBool()) return ancestor;
    }
    return origin->window();
}

QWidget *spatialNeighbor(QWidget *origin, GamepadNavigation::Action action) {
    auto *scope = navigationScope(origin);
    if (!origin || !scope) return nullptr;
    const auto originCenter = origin->mapToGlobal(origin->rect().center());
    QWidget *best = nullptr;
    double bestScore = std::numeric_limits<double>::max();
    auto candidates = scope->findChildren<QWidget *>();
    for (auto *candidate : candidates) {
        if (!candidate || candidate == origin || !candidate->isEnabled() ||
            !candidate->isVisibleTo(scope) ||
            !(candidate->focusPolicy() & Qt::TabFocus)) {
            continue;
        }
        const auto center = candidate->mapToGlobal(candidate->rect().center());
        const double dx = center.x() - originCenter.x();
        const double dy = center.y() - originCenter.y();
        const double primary = action == GamepadNavigation::MoveLeft ? -dx
            : action == GamepadNavigation::MoveRight ? dx
            : action == GamepadNavigation::MoveUp ? -dy : dy;
        if (primary <= 2.0) continue;
        const double perpendicular = (action == GamepadNavigation::MoveLeft ||
                                      action == GamepadNavigation::MoveRight)
            ? std::abs(dy) : std::abs(dx);
        const double score = primary + perpendicular * 3.0;
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}
}

GamepadNavigation::GamepadNavigation(QObject *parent,
                                     bool allowInactiveApplicationForTests)
    : QObject(parent),
      allowInactiveApplicationForTests_(allowInactiveApplicationForTests) {
    if (SDL_InitSubSystem(sdlSubsystemFlags) != 0) return;
    acquiredSubsystems_ = true;
    acquireSdlGlobalState();

    available_ = true;
    for (int index = 0; index < SDL_NumJoysticks(); ++index) openController(index);

    repeatClock_.start();
    pollTimer_ = new QTimer(this);
    pollTimer_->setTimerType(Qt::PreciseTimer);
    pollTimer_->setInterval(16);
    connect(pollTimer_, &QTimer::timeout, this, &GamepadNavigation::pollEvents);
    pollTimer_->start();
}

GamepadNavigation::~GamepadNavigation() {
    if (pollTimer_) pollTimer_->stop();
    for (auto *opaque : controllers_) {
        SDL_GameControllerClose(static_cast<SDL_GameController *>(opaque));
    }
    controllers_.clear();
    clearHeldActions();
    if (acquiredSubsystems_) {
        releaseSdlGlobalState();
        SDL_QuitSubSystem(sdlSubsystemFlags);
    }
}

bool GamepadNavigation::acceptsInput(Qt::ApplicationState state, bool hasActiveWindow,
                                     bool allowInactiveApplicationForTests) {
    return hasActiveWindow &&
        (state == Qt::ApplicationActive || allowInactiveApplicationForTests);
}

GamepadNavigation::Action GamepadNavigation::actionForButton(int button) {
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return MoveUp;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return MoveDown;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return MoveLeft;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return MoveRight;
    case SDL_CONTROLLER_BUTTON_A: return Activate;
    case SDL_CONTROLLER_BUTTON_B:
    case SDL_CONTROLLER_BUTTON_BACK: return Back;
    case SDL_CONTROLLER_BUTTON_X:
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return Secondary;
    case SDL_CONTROLLER_BUTTON_Y: return SearchOrFullscreen;
    case SDL_CONTROLLER_BUTTON_START: return PlayPause;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return PreviousPage;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return NextPage;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return Activate;
#if SDL_VERSION_ATLEAST(2, 0, 14)
    case SDL_CONTROLLER_BUTTON_MISC1: return SearchOrFullscreen;
    case SDL_CONTROLLER_BUTTON_PADDLE1: return PreviousPage;
    case SDL_CONTROLLER_BUTTON_PADDLE2: return NextPage;
    case SDL_CONTROLLER_BUTTON_PADDLE3: return PageUp;
    case SDL_CONTROLLER_BUTTON_PADDLE4: return PageDown;
    case SDL_CONTROLLER_BUTTON_TOUCHPAD: return Secondary;
#endif
    default: return NoAction;
    }
}

GamepadNavigation::Action GamepadNavigation::actionForAxis(int axis, int value) {
    if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
        return value >= axisDeadZone ? PageUp : NoAction;
    }
    if (axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
        return value >= axisDeadZone ? PageDown : NoAction;
    }
    if (value > -axisDeadZone && value < axisDeadZone) return NoAction;
    switch (axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
    case SDL_CONTROLLER_AXIS_RIGHTX:
        return value < 0 ? MoveLeft : MoveRight;
    case SDL_CONTROLLER_AXIS_LEFTY:
        return value < 0 ? MoveUp : MoveDown;
    case SDL_CONTROLLER_AXIS_RIGHTY:
        return value < 0 ? PageUp : PageDown;
    default:
        return NoAction;
    }
}

bool GamepadNavigation::repeats(Action action) {
    return action == MoveUp || action == MoveDown || action == MoveLeft ||
           action == MoveRight || action == PageUp || action == PageDown;
}

void GamepadNavigation::openController(int deviceIndex) {
    if (!SDL_IsGameController(deviceIndex)) return;
    auto *controller = SDL_GameControllerOpen(deviceIndex);
    if (!controller) return;
    auto *joystick = SDL_GameControllerGetJoystick(controller);
    const auto instanceId = SDL_JoystickInstanceID(joystick);
    if (instanceId < 0 || controllers_.contains(instanceId)) {
        SDL_GameControllerClose(controller);
        return;
    }
    controllers_.insert(instanceId, controller);
    emit controllerConnected(QString::fromUtf8(SDL_GameControllerName(controller)));
    emit controllerCountChanged(controllers_.size());
}

void GamepadNavigation::closeController(qint32 instanceId) {
    auto *controller = static_cast<SDL_GameController *>(controllers_.take(instanceId));
    if (!controller) return;
    clearControllerHeldActions(instanceId);
    SDL_GameControllerClose(controller);
    emit controllerDisconnected();
    emit controllerCountChanged(controllers_.size());
}

void GamepadNavigation::clearHeldActions() {
    buttonActions_.clear();
    axisActions_.clear();
    heldCounts_.clear();
    nextRepeatAt_.clear();
}

void GamepadNavigation::clearControllerHeldActions(qint32 instanceId) {
    const auto buttonKeys = buttonActions_.keys();
    for (const auto key : buttonKeys) {
        if (instanceFromControlKey(key) != instanceId) continue;
        updateHeld(buttonActions_.take(key), false);
    }
    const auto axisKeys = axisActions_.keys();
    for (const auto key : axisKeys) {
        if (instanceFromControlKey(key) != instanceId) continue;
        updateHeld(axisActions_.take(key), false);
    }
}

void GamepadNavigation::updateButton(qint32 instanceId, int button, bool pressed) {
    const auto key = controlKey(instanceId, button);
    if (pressed) {
        if (buttonActions_.contains(key)) return;
        const auto action = actionForButton(button);
        if (!repeats(action)) return;
        buttonActions_.insert(key, action);
        updateHeld(action, true);
        return;
    }
    const auto previous = buttonActions_.take(key);
    if (previous != NoAction) updateHeld(previous, false);
}

void GamepadNavigation::updateHeld(Action action, bool pressed) {
    if (action == NoAction || !repeats(action)) return;
    const auto key = int(action);
    if (pressed) {
        const auto count = heldCounts_.value(key);
        heldCounts_.insert(key, count + 1);
        if (count == 0) {
            dispatch(action);
            nextRepeatAt_.insert(key, repeatClock_.elapsed() + initialRepeatDelayMs);
        }
        return;
    }
    const auto count = heldCounts_.value(key);
    if (count <= 1) {
        heldCounts_.remove(key);
        nextRepeatAt_.remove(key);
    } else {
        heldCounts_.insert(key, count - 1);
    }
}

void GamepadNavigation::updateAxis(qint32 instanceId, int axis, int value) {
    const auto key = controlKey(instanceId, axis);
    const auto next = actionForAxis(axis, value);
    const auto previous = axisActions_.value(key, NoAction);
    if (next == previous) return;
    if (previous != NoAction) updateHeld(previous, false);
    if (next == NoAction) axisActions_.remove(key);
    else {
        axisActions_.insert(key, next);
        updateHeld(next, true);
    }
}

void GamepadNavigation::pollEvents() {
    SDL_Event event;
    const bool acceptsControllerInput = acceptsInput(
        QGuiApplication::applicationState(), QApplication::activeWindow() != nullptr,
        allowInactiveApplicationForTests_);
    if (!acceptsControllerInput) clearHeldActions();
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_CONTROLLERDEVICEADDED:
            openController(event.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            closeController(event.cdevice.which);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP: {
            if (!acceptsControllerInput) break;
            const auto action = actionForButton(event.cbutton.button);
            const bool pressed = event.type == SDL_CONTROLLERBUTTONDOWN;
            if (repeats(action)) {
                updateButton(event.cbutton.which, event.cbutton.button, pressed);
            }
            else if (pressed) dispatch(action);
            break;
        }
        case SDL_CONTROLLERAXISMOTION:
            if (acceptsControllerInput) {
                updateAxis(event.caxis.which, event.caxis.axis, event.caxis.value);
            }
            break;
        default:
            break;
        }
    }
    if (!acceptsControllerInput) return;
    const auto now = repeatClock_.elapsed();
    const auto actions = nextRepeatAt_.keys();
    for (const auto key : actions) {
        if (now < nextRepeatAt_.value(key)) continue;
        dispatch(Action(key));
        nextRepeatAt_.insert(key, now + repeatIntervalMs);
    }
}

void GamepadNavigation::sendKey(int key, Qt::KeyboardModifiers modifiers) {
    auto *target = QApplication::focusWidget();
    if (!target) target = QApplication::activeWindow();
    if (!target) return;
    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(target, &press);
    QApplication::sendEvent(target, &release);
}

void GamepadNavigation::dispatchDirectional(Action action) {
    auto *target = QApplication::focusWidget();
    if (!target) target = QApplication::activeWindow();
    if (!target) return;
    const auto key = action == MoveUp ? Qt::Key_Up
                   : action == MoveDown ? Qt::Key_Down
                   : action == MoveLeft ? Qt::Key_Left : Qt::Key_Right;
    if (auto *view = qobject_cast<QAbstractItemView *>(target)) {
        const auto previous = view->currentIndex();
        sendKey(key);
        if (view->currentIndex().isValid() && view->currentIndex() != previous) return;
    }
    if (qobject_cast<QMenu *>(target) || qobject_cast<QAbstractSlider *>(target) ||
        qobject_cast<QComboBox *>(target) || qobject_cast<QTabBar *>(target) ||
        qobject_cast<QTextEdit *>(target) ||
        (qobject_cast<QLineEdit *>(target) &&
         (action == MoveLeft || action == MoveRight))) {
        sendKey(key);
        return;
    }

    auto *next = spatialNeighbor(target, action);
    if (!next) return;
    next->setFocus(Qt::TabFocusReason);
    for (auto *ancestor = next->parentWidget(); ancestor;
         ancestor = ancestor->parentWidget()) {
        if (auto *scroll = qobject_cast<QScrollArea *>(ancestor)) {
            scroll->ensureWidgetVisible(next, 24, 24);
            break;
        }
    }
}

void GamepadNavigation::dispatch(Action action) {
    if (action == NoAction) return;
    emit actionTriggered(action);
    switch (action) {
    case MoveUp:
    case MoveDown:
    case MoveLeft:
    case MoveRight:
        dispatchDirectional(action);
        break;
    case Activate:
        if (auto *button = qobject_cast<QAbstractButton *>(QApplication::focusWidget())) {
            button->click();
        } else {
            sendKey(Qt::Key_Return);
        }
        break;
    case Back:
        sendKey(Qt::Key_Escape);
        break;
    case PlayPause:
        sendKey(Qt::Key_Space);
        break;
    case PageUp:
        sendKey(Qt::Key_PageUp);
        break;
    case PageDown:
        sendKey(Qt::Key_PageDown);
        break;
    case Secondary:
    case SearchOrFullscreen:
    case PreviousPage:
    case NextPage:
    case NoAction:
        break;
    }
}

} // namespace CloudStream
