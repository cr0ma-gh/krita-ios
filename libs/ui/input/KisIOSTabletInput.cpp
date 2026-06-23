/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisIOSTabletInput.h"

#include "KisIOSTabletBridge.h"

#include <QApplication>
#include <QPoint>
#include <QPointer>
#include <QTabletEvent>
#include <QWidget>

namespace
{
// A single shared stylus device description for the synthesised events.
const QPointingDevice *iosStylusDevice()
{
    static const QPointingDevice device(
        QStringLiteral("Apple Pencil"),
        /* systemId   */ 1,
        QInputDevice::DeviceType::Stylus,
        QPointingDevice::PointerType::Pen,
        QInputDevice::Capability::Position | QInputDevice::Capability::Pressure
            | QInputDevice::Capability::XTilt | QInputDevice::Capability::YTilt,
        /* maxPoints  */ 1,
        /* buttonCount*/ 1);
    return &device;
}

QEvent::Type phaseToType(KisIOSPenSample::Phase phase)
{
    switch (phase) {
    case KisIOSPenSample::Begin:
        return QEvent::TabletPress;
    case KisIOSPenSample::End:
    case KisIOSPenSample::Cancel:
        return QEvent::TabletRelease;
    case KisIOSPenSample::Move:
    default:
        return QEvent::TabletMove;
    }
}
} // namespace

void KisIOSTabletInput::install(QWidget *canvas)
{
    if (!canvas) {
        return;
    }

    QPointer<QWidget> target(canvas);

    KisIOSTabletBridge::install(canvas, [target](const KisIOSPenSample &s) {
        if (!target) {
            return;
        }

        // Bridge samples are in the window's native-view coordinates (points,
        // which already match Qt's logical coordinates). Map them into the
        // canvas widget's local space before delivering the event.
        QWidget *window = target->window();
        const QPoint windowPos(qRound(s.x), qRound(s.y));
        const QPointF local = target->mapFrom(window, windowPos);
        const QPointF global = window->mapToGlobal(windowPos);

        const bool down =
            (s.phase != KisIOSPenSample::End && s.phase != KisIOSPenSample::Cancel);
        const Qt::MouseButton button = Qt::LeftButton;
        const Qt::MouseButtons buttons = down ? Qt::LeftButton : Qt::NoButton;

        QTabletEvent ev(phaseToType(s.phase),
                        iosStylusDevice(),
                        local,
                        global,
                        s.pressure,                          // 0..1
                        static_cast<float>(s.tiltX),
                        static_cast<float>(s.tiltY),
                        /* tangentialPressure */ 0.0f,
                        /* rotation           */ 0.0,
                        /* z                  */ 0.0f,
                        Qt::NoModifier,
                        button,
                        buttons);

        // Bridge callbacks run on the UIKit main thread, which is Qt's GUI
        // thread on iOS, so a synchronous send is safe (and avoids heap churn).
        QApplication::sendEvent(target, &ev);
    });
}
