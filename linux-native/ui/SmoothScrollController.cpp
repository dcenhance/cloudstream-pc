#include "SmoothScrollController.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QEvent>
#include <QScrollBar>
#include <QScreen>
#include <QScroller>
#include <QScrollerProperties>
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>
#include <algorithm>
#include <cmath>

namespace CloudStream {
namespace {
constexpr auto installedProperty = "cloudstreamSmoothScrollInstalled";
constexpr auto horizontalProperty = "smoothHorizontalWheel";
constexpr double horizontalInputStrength = 0.5;
}

SmoothScrollController::SmoothScrollController(QAbstractScrollArea *area, WheelMode mode)
    : QObject(area), area_(area), mode_(mode) {
    setObjectName("cloudstreamSmoothScrollController");
    horizontalMotion_.position = horizontalMotion_.target = area->horizontalScrollBar()->value();
    verticalMotion_.position = verticalMotion_.target = area->verticalScrollBar()->value();
    motionTimer_ = new QTimer(this);
    motionTimer_->setTimerType(Qt::PreciseTimer);
    motionTimer_->setInterval(16);
    connect(motionTimer_, &QTimer::timeout, this, &SmoothScrollController::updateMotion);
    area->viewport()->installEventFilter(this);
    const auto synchronizeExternalChange = [this](QScrollBar *bar, int position) {
        if (!applyingMotion_) stopMotion(bar, position);
    };
    connect(area->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            [this, synchronizeExternalChange](int position) {
                synchronizeExternalChange(area_->horizontalScrollBar(), position);
            });
    connect(area->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this, synchronizeExternalChange](int position) {
                synchronizeExternalChange(area_->verticalScrollBar(), position);
            });

    if (auto *view = qobject_cast<QAbstractItemView *>(area)) {
        view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    }

    QScroller::grabGesture(area->viewport(), QScroller::TouchGesture);
    auto properties = QScroller::scroller(area->viewport())->scrollerProperties();
    properties.setScrollMetric(QScrollerProperties::DragVelocitySmoothingFactor, 0.82);
    properties.setScrollMetric(QScrollerProperties::DecelerationFactor, 0.045);
    properties.setScrollMetric(QScrollerProperties::MaximumVelocity, 1.15);
    properties.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.22);
    properties.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.05);
    properties.setScrollMetric(QScrollerProperties::FrameRate, QScrollerProperties::Fps60);
    QScroller::scroller(area->viewport())->setScrollerProperties(properties);
}

void SmoothScrollController::attach(QAbstractScrollArea *area, WheelMode mode) {
    if (!area || area->property(installedProperty).toBool()) return;
    area->setProperty(installedProperty, true);
    new SmoothScrollController(area, mode);
}

void SmoothScrollController::attachRecursively(QWidget *root) {
    if (!root) return;
    if (auto *area = qobject_cast<QAbstractScrollArea *>(root)) {
        attach(area, area->property(horizontalProperty).toBool() ? HorizontalWheel : Automatic);
    }
    const auto areas = root->findChildren<QAbstractScrollArea *>();
    for (auto *area : areas) {
        attach(area, area->property(horizontalProperty).toBool() ? HorizontalWheel : Automatic);
    }
}

int SmoothScrollController::frameIntervalForRefreshRate(qreal refreshRate) {
    if (!std::isfinite(refreshRate) || refreshRate < 30.0) return 16;
    return std::clamp(qRound(1000.0 / refreshRate), 4, 17);
}

SmoothScrollController::AxisMotion &SmoothScrollController::motionFor(QScrollBar *bar) {
    return bar == area_->horizontalScrollBar() ? horizontalMotion_ : verticalMotion_;
}

const SmoothScrollController::AxisMotion &SmoothScrollController::motionFor(QScrollBar *bar) const {
    return bar == area_->horizontalScrollBar() ? horizontalMotion_ : verticalMotion_;
}

void SmoothScrollController::animate(QScrollBar *bar, int delta) {
    if (!bar || delta == 0 || bar->maximum() <= bar->minimum()) return;
    auto &motion = motionFor(bar);
    if (!motion.active) {
        motion.position = bar->value();
        motion.target = bar->value();
        motion.velocity = 0.0;
    } else if (delta * motion.velocity < 0.0) {
        motion.velocity *= 0.35;
    }
    const auto next = std::clamp(motion.target + delta,
                                 double(bar->minimum()), double(bar->maximum()));
    if (std::abs(next - motion.target) < 0.01) return;
    motion.target = next;
    motion.active = true;
    if (!motionTimer_->isActive()) {
        qreal refreshRate = 0.0;
        if (area_->window() && area_->window()->windowHandle() &&
            area_->window()->windowHandle()->screen()) {
            refreshRate = area_->window()->windowHandle()->screen()->refreshRate();
        } else if (QGuiApplication::primaryScreen()) {
            refreshRate = QGuiApplication::primaryScreen()->refreshRate();
        }
        motionTimer_->setInterval(frameIntervalForRefreshRate(refreshRate));
        motionClock_.start();
        motionTimer_->start();
    }
}

void SmoothScrollController::stopMotion(QScrollBar *bar, int position) {
    if (!bar) return;
    auto &motion = motionFor(bar);
    motion.position = position;
    motion.target = position;
    motion.velocity = 0.0;
    motion.active = false;
    if (!horizontalMotion_.active && !verticalMotion_.active) motionTimer_->stop();
}

void SmoothScrollController::updateMotion() {
    if (!area_) {
        motionTimer_->stop();
        return;
    }
    const auto dt = std::clamp(motionClock_.nsecsElapsed() / 1000000000.0,
                               0.001, 0.1);
    motionClock_.restart();
    const auto advance = [dt](QScrollBar *bar, AxisMotion &motion) {
        if (!motion.active || !bar) return;
        motion.target = std::clamp(motion.target,
                                   double(bar->minimum()), double(bar->maximum()));
        constexpr double frequency = 11.0;
        const auto offset = motion.position - motion.target;
        const auto coefficient = motion.velocity + frequency * offset;
        const auto decay = std::exp(-frequency * dt);
        motion.position = motion.target + (offset + coefficient * dt) * decay;
        motion.velocity = (motion.velocity - frequency * coefficient * dt) * decay;
        motion.position = std::clamp(motion.position,
                                     double(bar->minimum()), double(bar->maximum()));
        bar->setValue(int(std::round(motion.position)));
        if (std::abs(motion.target - motion.position) < 0.35 &&
            std::abs(motion.velocity) < 3.0) {
            motion.position = motion.target;
            bar->setValue(int(std::round(motion.target)));
            motion.velocity = 0.0;
            motion.active = false;
        }
    };
    applyingMotion_ = true;
    advance(area_->horizontalScrollBar(), horizontalMotion_);
    advance(area_->verticalScrollBar(), verticalMotion_);
    applyingMotion_ = false;
    if (!horizontalMotion_.active && !verticalMotion_.active) motionTimer_->stop();
}

void SmoothScrollController::scrollBy(QAbstractScrollArea *area,
                                      int horizontalPixels, int verticalPixels) {
    if (!area) return;
    attach(area, area->property(horizontalProperty).toBool() ? HorizontalWheel : Automatic);
    auto *controller = area->findChild<SmoothScrollController *>(
        "cloudstreamSmoothScrollController", Qt::FindDirectChildrenOnly);
    if (!controller) return;
    if (horizontalPixels) controller->animate(area->horizontalScrollBar(), horizontalPixels);
    if (verticalPixels) controller->animate(area->verticalScrollBar(), verticalPixels);
}

bool SmoothScrollController::eventFilter(QObject *watched, QEvent *event) {
    if (!area_ || watched != area_->viewport() || event->type() != QEvent::Wheel) {
        return QObject::eventFilter(watched, event);
    }
    auto *wheel = static_cast<QWheelEvent *>(event);
    if (wheel->modifiers().testFlag(Qt::ControlModifier)) return false;

    const auto pixel = wheel->pixelDelta();
    const auto angle = wheel->angleDelta();
    const bool shiftHorizontal = wheel->modifiers().testFlag(Qt::ShiftModifier);
    const bool verticalGesture = !shiftHorizontal &&
        ((!pixel.isNull() && std::abs(pixel.y()) >= std::abs(pixel.x())) ||
         (pixel.isNull() && std::abs(angle.y()) >= std::abs(angle.x())));
    if (mode_ == HorizontalWheel && verticalGesture) {
        for (QWidget *ancestor = area_->parentWidget(); ancestor;
             ancestor = ancestor->parentWidget()) {
            auto *parentArea = qobject_cast<QAbstractScrollArea *>(ancestor);
            if (!parentArea || parentArea->verticalScrollBar()->maximum() <=
                                   parentArea->verticalScrollBar()->minimum()) {
                continue;
            }
            attach(parentArea, Automatic);
            auto *target = parentArea->viewport();
            const QPointF localPosition = target->mapFromGlobal(
                wheel->globalPosition().toPoint());
            QWheelEvent forwarded(localPosition, wheel->globalPosition(), pixel, angle,
                                  wheel->buttons(), wheel->modifiers(), wheel->phase(),
                                  wheel->inverted());
            forwarded.ignore();
            QApplication::sendEvent(target, &forwarded);
            wheel->accept();
            return true;
        }
        wheel->accept();
        return true;
    }
    bool horizontal = mode_ == HorizontalWheel ||
        shiftHorizontal ||
        std::abs(pixel.x()) > std::abs(pixel.y()) ||
        (pixel.isNull() && std::abs(angle.x()) > std::abs(angle.y()));
    auto *bar = horizontal ? area_->horizontalScrollBar() : area_->verticalScrollBar();
    if (!bar || bar->maximum() <= bar->minimum()) {
        if (horizontal && mode_ == Automatic) {
            horizontal = false;
            bar = area_->verticalScrollBar();
        }
        if (!bar || bar->maximum() <= bar->minimum()) return false;
    }

    int delta = 0;
    if (!pixel.isNull()) {
        const auto raw = std::abs(pixel.x()) > std::abs(pixel.y())
            ? pixel.x() : pixel.y();
        if (horizontal) {
            const auto scaled = -raw * horizontalInputStrength + horizontalPixelRemainder_;
            delta = int(scaled);
            horizontalPixelRemainder_ = scaled - delta;
        } else {
            delta = -raw;
        }
        if (delta == 0) {
            wheel->accept();
            return true;
        }
        const auto next = std::clamp(bar->value() + delta, bar->minimum(), bar->maximum());
        if (next == bar->value()) {
            if (horizontal) horizontalPixelRemainder_ = 0.0;
            return false;
        }
        bar->setValue(next);
        stopMotion(bar, next);
    } else {
        const auto raw = std::abs(angle.x()) > std::abs(angle.y())
            ? angle.x() : angle.y();
        if (raw == 0) return false;
        const auto pixelsPerNotch = horizontal ? 310.0 * horizontalInputStrength : 150.0;
        delta = int(std::round((-raw / 120.0) * pixelsPerNotch));
        const auto &motion = motionFor(bar);
        const auto base = motion.active ? motion.target : bar->value();
        const auto next = std::clamp(base + delta,
                                     double(bar->minimum()), double(bar->maximum()));
        if (next == base) return false;
        animate(bar, delta);
    }
    wheel->accept();
    return true;
}

} // namespace CloudStream
