#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>

class QAbstractScrollArea;
class QScrollBar;
class QTimer;

namespace CloudStream {

class SmoothScrollController final : public QObject {
    Q_OBJECT
public:
    enum WheelMode {
        Automatic,
        HorizontalWheel,
    };

    static void attach(QAbstractScrollArea *area, WheelMode mode = Automatic);
    static void attachRecursively(QWidget *root);
    static void scrollBy(QAbstractScrollArea *area, int horizontalPixels, int verticalPixels);
    static int frameIntervalForRefreshRate(qreal refreshRate);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct AxisMotion {
        double position = 0.0;
        double target = 0.0;
        double velocity = 0.0;
        bool active = false;
    };

    SmoothScrollController(QAbstractScrollArea *area, WheelMode mode);
    void animate(QScrollBar *bar, int delta);
    void stopMotion(QScrollBar *bar, int position);
    void updateMotion();
    AxisMotion &motionFor(QScrollBar *bar);
    const AxisMotion &motionFor(QScrollBar *bar) const;

    QPointer<QAbstractScrollArea> area_;
    WheelMode mode_ = Automatic;
    QTimer *motionTimer_{};
    QElapsedTimer motionClock_;
    AxisMotion horizontalMotion_;
    AxisMotion verticalMotion_;
    double horizontalPixelRemainder_ = 0.0;
    bool applyingMotion_ = false;
};

} // namespace CloudStream
