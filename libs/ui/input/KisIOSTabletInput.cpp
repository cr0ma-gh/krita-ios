/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisIOSTabletInput.h"

#include "KisIOSTabletBridge.h"

#include <QAction>
#include <QApplication>
#include <QPoint>
#include <QPointer>
#include <QTabletEvent>
#include <QWidget>

#include <KisPart.h>
#include <KisMainWindow.h>
#include <KisView.h>
#include <kactioncollection.h>
#include <kis_canvas2.h>
#include <kis_popup_palette.h>

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
    case KisIOSPenSample::Hover:   // proximity move: same event, but no button
    case KisIOSPenSample::Move:
    default:
        return QEvent::TabletMove;
    }
}

// --- Apple Pencil double-tap -> Krita action -------------------------------
// Runs on the GUI thread (the bridge invokes the tap sink there). Resolves the
// action lazily at tap time, so it always targets the currently active window.

void triggerNamedAction(const QString &name)
{
    KisMainWindow *win = KisPart::instance()->currentMainwindow();
    if (!win) {
        return;
    }
    KisKActionCollection *actions = win->actionCollection();
    QAction *action = actions ? actions->action(name) : nullptr;
    if (action) {
        action->trigger();
    } else {
        qWarning("KisIOSTabletInput: no action '%s' for Apple Pencil double-tap",
                 qUtf8Printable(name));
    }
}

void toggleColorPalette()
{
    KisMainWindow *win = KisPart::instance()->currentMainwindow();
    KisView *view = win ? win->activeView() : nullptr;
    KisCanvas2 *canvas = view ? view->canvasBase() : nullptr;
    KisPopupPalette *palette = canvas ? canvas->popupPalette() : nullptr;
    if (!palette) {
        return;
    }
    if (palette->onScreen()) {
        palette->dismiss();
        return;
    }
    QWidget *cw = canvas->canvasWidget();
    const QPoint center = cw ? QPoint(cw->width() / 2, cw->height() / 2) : QPoint();
    palette->popup(center);
}

// Maps the user's iOS system preference to the closest Krita behaviour. Actions
// with no Krita analogue (ShowInkAttributes) and an unreadable preference fall
// back to the eraser toggle — which is also Apple's own default double-tap.
void handlePencilTap(KisIOSPencilTapAction action)
{
    switch (action) {
    case KisIOSPencilTapAction::Ignore:
        break;
    case KisIOSPencilTapAction::SwitchPrevious:
        triggerNamedAction(QStringLiteral("previous_preset"));
        break;
    case KisIOSPencilTapAction::ShowColorPalette:
        toggleColorPalette();
        break;
    case KisIOSPencilTapAction::ToggleEraser:
    case KisIOSPencilTapAction::ShowInkAttributes:
    case KisIOSPencilTapAction::Unknown:
    default:
        triggerNamedAction(QStringLiteral("erase_action"));
        break;
    }
}
} // namespace

void KisIOSTabletInput::install(QWidget *canvas)
{
    if (!canvas) {
        return;
    }

    // The Apple Pencil double-tap is app-global (a property of the Pencil, not
    // of a canvas), so register its handler once regardless of canvas switches.
    static bool tapWired = false;
    if (!tapWired) {
        tapWired = true;
        KisIOSTabletBridge::setPencilTapSink(&handlePencilTap);
    }

    QPointer<QWidget> target(canvas);

    KisIOSTabletBridge::install(canvas, [target](const KisIOSPenSample &s) {
        if (!target) {
            return;
        }

        // Palm rejection: only the Apple Pencil draws. Finger touches are left
        // to Qt's native touch pipeline (Krita's pan/zoom/gesture handling), so
        // a resting palm or a stray finger never lays down a stroke. This is the
        // standard iPad model — the Pencil paints, fingers gesture.
        if (!s.isPencil) {
            return;
        }

        // Bridge samples are in the window's native-view coordinates (points,
        // which already match Qt's logical coordinates). Map them into the
        // canvas widget's local space before delivering the event.
        QWidget *window = target->window();
        const QPoint windowPos(qRound(s.x), qRound(s.y));
        const QPointF local = target->mapFrom(window, windowPos);
        const QPointF global = window->mapToGlobal(windowPos);

        // "Down" (a button held) means the pen is in contact and painting.
        // Hover moves the cursor with the pen in proximity but no button, so
        // Krita shows the live brush outline without drawing.
        const bool down =
            (s.phase == KisIOSPenSample::Begin || s.phase == KisIOSPenSample::Move);
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
