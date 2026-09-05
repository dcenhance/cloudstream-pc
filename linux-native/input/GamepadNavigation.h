#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>

class QTimer;

namespace CloudStream {

class GamepadNavigation final : public QObject {
    Q_OBJECT
public:
    enum Action {
        NoAction,
        MoveUp,
        MoveDown,
        MoveLeft,
        MoveRight,
        Activate,
        Back,
        Secondary,
        SearchOrFullscreen,
        PlayPause,
        PreviousPage,
        NextPage,
        PageUp,
        PageDown,
    };
    Q_ENUM(Action)

    explicit GamepadNavigation(QObject *parent = nullptr,
                               bool allowInactiveApplicationForTests = false);
    ~GamepadNavigation() override;

    static Action actionForButton(int button);
    static Action actionForAxis(int axis, int value);
    static bool acceptsInput(Qt::ApplicationState state, bool hasActiveWindow,
                             bool allowInactiveApplicationForTests = false);

    bool isAvailable() const { return available_; }
    int controllerCount() const { return controllers_.size(); }
    void dispatch(Action action);

signals:
    void actionTriggered(CloudStream::GamepadNavigation::Action action);
    void controllerConnected(const QString &name);
    void controllerDisconnected();
    void controllerCountChanged(int count);

private:
    static bool repeats(Action action);
    void pollEvents();
    void openController(int deviceIndex);
    void closeController(qint32 instanceId);
    void updateButton(qint32 instanceId, int button, bool pressed);
    void updateHeld(Action action, bool pressed);
    void updateAxis(qint32 instanceId, int axis, int value);
    void clearControllerHeldActions(qint32 instanceId);
    void dispatchDirectional(Action action);
    void sendKey(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void clearHeldActions();

    QTimer *pollTimer_{};
    QElapsedTimer repeatClock_;
    QHash<qint32, void *> controllers_;
    QHash<quint64, Action> buttonActions_;
    QHash<quint64, Action> axisActions_;
    QHash<int, int> heldCounts_;
    QHash<int, qint64> nextRepeatAt_;
    bool available_ = false;
    bool acquiredSubsystems_ = false;
    bool allowInactiveApplicationForTests_ = false;
};

} // namespace CloudStream
