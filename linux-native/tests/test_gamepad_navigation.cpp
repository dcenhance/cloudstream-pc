#include "../input/GamepadNavigation.h"

#include <QtTest>

#include <QDialog>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QVBoxLayout>
#include <SDL.h>

class GamepadNavigationTest final : public QObject {
    Q_OBJECT

    static void activateForTest(QWidget *window) {
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        QApplication::setActiveWindow(window);
        QT_WARNING_POP
    }

private slots:
    void mapsStandardControllerButtons() {
        using Navigation = CloudStream::GamepadNavigation;
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_DPAD_UP), Navigation::MoveUp);
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN), Navigation::MoveDown);
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT), Navigation::MoveLeft);
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT), Navigation::MoveRight);
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_A), Navigation::Activate);
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_B), Navigation::Back);
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_X), Navigation::Secondary);
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_Y), Navigation::SearchOrFullscreen);
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_START), Navigation::PlayPause);
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER), Navigation::PreviousPage);
        QCOMPARE(Navigation::actionForButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER), Navigation::NextPage);
    }

    void mapsSticksAndTriggersWithADeadZone() {
        using Navigation = CloudStream::GamepadNavigation;
        QCOMPARE(Navigation::actionForAxis(SDL_CONTROLLER_AXIS_LEFTX, 8000), Navigation::NoAction);
        QCOMPARE(Navigation::actionForAxis(SDL_CONTROLLER_AXIS_LEFTX, -20000), Navigation::MoveLeft);
        QCOMPARE(Navigation::actionForAxis(SDL_CONTROLLER_AXIS_LEFTX, 20000), Navigation::MoveRight);
        QCOMPARE(Navigation::actionForAxis(SDL_CONTROLLER_AXIS_LEFTY, -20000), Navigation::MoveUp);
        QCOMPARE(Navigation::actionForAxis(SDL_CONTROLLER_AXIS_LEFTY, 20000), Navigation::MoveDown);
        QCOMPARE(Navigation::actionForAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 24000), Navigation::PageUp);
        QCOMPARE(Navigation::actionForAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 24000), Navigation::PageDown);
    }

    void rejectsPolledInputWhenTheApplicationIsInactive() {
        using Navigation = CloudStream::GamepadNavigation;
        QVERIFY(Navigation::acceptsInput(Qt::ApplicationActive, true));
        QVERIFY(!Navigation::acceptsInput(Qt::ApplicationInactive, true));
        QVERIFY(!Navigation::acceptsInput(Qt::ApplicationActive, false));
        QVERIFY(Navigation::acceptsInput(Qt::ApplicationInactive, true, true));
    }

    void activateClicksTheFocusedButton() {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *button = new QPushButton("Play");
        layout->addWidget(button);
        window.show();
        activateForTest(&window);
        button->setFocus();
        QApplication::processEvents();
        QSignalSpy clicked(button, &QPushButton::clicked);
        CloudStream::GamepadNavigation navigation;

        navigation.dispatch(CloudStream::GamepadNavigation::Activate);

        QCOMPARE(clicked.size(), 1);
    }

    void dpadUsesSimpleFourWayGeometryInsteadOfTabOrder() {
        QWidget window;
        window.resize(360, 240);
        auto *origin = new QPushButton("Origin", &window);
        auto *down = new QPushButton("Down", &window);
        auto *right = new QPushButton("Right", &window);
        auto *diagonal = new QPushButton("Diagonal", &window);
        origin->setGeometry(20, 20, 120, 50);
        down->setGeometry(20, 140, 120, 50);
        right->setGeometry(210, 20, 120, 50);
        diagonal->setGeometry(210, 140, 120, 50);
        window.show();
        activateForTest(&window);
        origin->setFocus();
        QApplication::processEvents();
        CloudStream::GamepadNavigation navigation;

        navigation.dispatch(CloudStream::GamepadNavigation::MoveRight);
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(right));
        navigation.dispatch(CloudStream::GamepadNavigation::MoveDown);
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(diagonal));
        navigation.dispatch(CloudStream::GamepadNavigation::MoveLeft);
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(down));
        navigation.dispatch(CloudStream::GamepadNavigation::MoveUp);
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(origin));
    }

    void dpadStaysInsideAnEmbeddedNavigationScope() {
        QWidget window;
        window.resize(520, 260);
        auto *scope = new QWidget(&window);
        scope->setGeometry(0, 0, 520, 260);
        scope->setProperty("controllerNavigationScope", true);
        scope->setFocusPolicy(Qt::StrongFocus);
        auto *origin = new QPushButton("Origin", scope);
        auto *inside = new QPushButton("Inside", scope);
        auto *behind = new QPushButton("Behind overlay", &window);
        origin->setGeometry(20, 90, 120, 50);
        inside->setGeometry(370, 90, 120, 50);
        behind->setGeometry(180, 90, 150, 50);
        scope->raise();
        window.show();
        activateForTest(&window);
        origin->setFocus();
        QApplication::processEvents();
        CloudStream::GamepadNavigation navigation;

        navigation.dispatch(CloudStream::GamepadNavigation::MoveRight);

        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(inside));
    }

    void dpadTraversesButtonsAndNavigatesLists() {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *first = new QPushButton("First");
        auto *second = new QPushButton("Second");
        auto *list = new QListWidget;
        list->addItems({"One", "Two", "Three"});
        layout->addWidget(first);
        layout->addWidget(second);
        layout->addWidget(list);
        window.show();
        activateForTest(&window);
        first->setFocus();
        QApplication::processEvents();
        CloudStream::GamepadNavigation navigation;

        navigation.dispatch(CloudStream::GamepadNavigation::MoveDown);
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(second));

        list->setCurrentRow(0);
        list->setFocus();
        navigation.dispatch(CloudStream::GamepadNavigation::MoveDown);
        QCOMPARE(list->currentRow(), 1);
    }

    void backSendsEscapeToTheActiveDialog() {
        QDialog dialog;
        dialog.show();
        activateForTest(&dialog);
        dialog.setFocus();
        QApplication::processEvents();
        CloudStream::GamepadNavigation navigation;

        navigation.dispatch(CloudStream::GamepadNavigation::Back);
        QApplication::processEvents();

        QVERIFY(!dialog.isVisible());
    }

    void dpadNavigatesEmbeddedMenusOnWayland() {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *menu = new QMenu(&window);
        menu->setWindowFlags(Qt::Widget);
        layout->addWidget(menu);
        auto *first = menu->addAction("First");
        auto *second = menu->addAction("Second");
        window.show();
        activateForTest(&window);
        menu->setFocus();
        menu->setActiveAction(first);
        QApplication::processEvents();
        CloudStream::GamepadNavigation navigation;

        navigation.dispatch(CloudStream::GamepadNavigation::MoveDown);

        QCOMPARE(menu->activeAction(), second);
    }

    void secondaryActionIsEmittedExactlyOnce() {
        CloudStream::GamepadNavigation navigation;
        QSignalSpy actions(&navigation, &CloudStream::GamepadNavigation::actionTriggered);

        navigation.dispatch(CloudStream::GamepadNavigation::Secondary);

        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions.first().first().value<CloudStream::GamepadNavigation::Action>(),
                 CloudStream::GamepadNavigation::Secondary);
    }

    void unpluggingOneControllerPreservesAnotherControllersHeldAction() {
        constexpr Uint32 subsystems =
            SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS;
        QVERIFY2(SDL_InitSubSystem(subsystems) == 0, SDL_GetError());
        int existingControllers = 0;
        for (int index = 0; index < SDL_NumJoysticks(); ++index) {
            if (SDL_IsGameController(index)) ++existingControllers;
        }
        const auto firstDevice = SDL_JoystickAttachVirtual(
            SDL_JOYSTICK_TYPE_GAMECONTROLLER,
            SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        const auto secondDevice = SDL_JoystickAttachVirtual(
            SDL_JOYSTICK_TYPE_GAMECONTROLLER,
            SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        QVERIFY2(firstDevice >= 0 && secondDevice >= 0, SDL_GetError());
        auto *firstJoystick = SDL_JoystickOpen(firstDevice);
        auto *secondJoystick = SDL_JoystickOpen(secondDevice);
        QVERIFY2(firstJoystick && secondJoystick, SDL_GetError());

        const auto addMapping = [](SDL_Joystick *joystick) {
            char guid[33]{};
            SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), guid, sizeof(guid));
            const auto mapping = QByteArray(guid) +
                ",CloudStream Multi Controller,a:b0,b:b1,x:b2,y:b3,"
                "back:b4,start:b6,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,"
                "leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5";
            return SDL_GameControllerAddMapping(mapping.constData()) >= 0;
        };
        QVERIFY2(addMapping(firstJoystick), SDL_GetError());
        QVERIFY2(addMapping(secondJoystick), SDL_GetError());

        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        layout->addWidget(new QPushButton("Focus"));
        window.show();
        activateForTest(&window);
        QApplication::processEvents();
        CloudStream::GamepadNavigation navigation(nullptr, true);
        QTRY_COMPARE_WITH_TIMEOUT(navigation.controllerCount(), existingControllers + 2, 1000);
        QSignalSpy actions(&navigation, &CloudStream::GamepadNavigation::actionTriggered);

        QVERIFY(SDL_JoystickSetVirtualAxis(firstJoystick, SDL_CONTROLLER_AXIS_LEFTX, 24000) == 0);
        QVERIFY(SDL_JoystickSetVirtualAxis(secondJoystick, SDL_CONTROLLER_AXIS_LEFTX, 24000) == 0);
        QTRY_VERIFY_WITH_TIMEOUT(!actions.isEmpty(), 1000);

        SDL_JoystickClose(firstJoystick);
        QVERIFY(SDL_JoystickDetachVirtual(firstDevice) == 0);
        QTRY_COMPARE_WITH_TIMEOUT(navigation.controllerCount(), existingControllers + 1, 1000);
        const int actionCountAfterDetach = actions.size();
        QTRY_VERIFY_WITH_TIMEOUT(actions.size() > actionCountAfterDetach, 1000);

        QVERIFY(SDL_JoystickSetVirtualAxis(secondJoystick, SDL_CONTROLLER_AXIS_LEFTX, 0) == 0);
        const auto secondInstance = SDL_JoystickInstanceID(secondJoystick);
        SDL_JoystickClose(secondJoystick);
        int currentSecondDevice = -1;
        for (int index = 0; index < SDL_NumJoysticks(); ++index) {
            if (SDL_JoystickGetDeviceInstanceID(index) == secondInstance) {
                currentSecondDevice = index;
                break;
            }
        }
        QVERIFY(currentSecondDevice >= 0);
        QVERIFY(SDL_JoystickDetachVirtual(currentSecondDevice) == 0);
        SDL_QuitSubSystem(subsystems);
    }

    void pollsARealSdlControllerEventPathAndPreservesOuterSdlOwner() {
        constexpr Uint32 subsystems =
            SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS;
        QVERIFY2(SDL_InitSubSystem(subsystems) == 0, SDL_GetError());
        int existingControllers = 0;
        for (int index = 0; index < SDL_NumJoysticks(); ++index) {
            if (SDL_IsGameController(index)) ++existingControllers;
        }
        const auto deviceIndex = SDL_JoystickAttachVirtual(
            SDL_JOYSTICK_TYPE_GAMECONTROLLER,
            SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        QVERIFY2(deviceIndex >= 0, SDL_GetError());
        auto *joystick = SDL_JoystickOpen(deviceIndex);
        QVERIFY2(joystick, SDL_GetError());

        char guid[33]{};
        SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), guid, sizeof(guid));
        const auto mapping = QByteArray(guid) +
            ",CloudStream Virtual Controller,a:b0,b:b1,x:b2,y:b3,"
            "back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,"
            "leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,"
            "dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,"
            "lefttrigger:a4,righttrigger:a5";
        QVERIFY2(SDL_GameControllerAddMapping(mapping.constData()) >= 0, SDL_GetError());
        QVERIFY(SDL_IsGameController(deviceIndex));

        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *button = new QPushButton("Activate");
        layout->addWidget(button);
        window.show();
        activateForTest(&window);
        button->setFocus();
        QApplication::processEvents();
        QSignalSpy clicked(button, &QPushButton::clicked);
        {
            CloudStream::GamepadNavigation navigation(nullptr, true);
            QCOMPARE(navigation.controllerCount(), existingControllers + 1);
            QVERIFY(SDL_JoystickSetVirtualButton(joystick, 0, SDL_PRESSED) == 0);
            QTRY_COMPARE_WITH_TIMEOUT(clicked.size(), 1, 1000);
            QVERIFY(SDL_JoystickSetVirtualButton(joystick, 0, SDL_RELEASED) == 0);
            QApplication::processEvents();
        }
        QCOMPARE(SDL_WasInit(subsystems) & subsystems, subsystems);

        SDL_JoystickClose(joystick);
        QVERIFY(SDL_JoystickDetachVirtual(deviceIndex) == 0);
        SDL_QuitSubSystem(subsystems);
    }
};

QTEST_MAIN(GamepadNavigationTest)
#include "test_gamepad_navigation.moc"
