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
#include <QVariant>
#include <QWidget>

#include <KisPart.h>
#include <KisMainWindow.h>
#include <KisView.h>
#include <KisUsageLogger.h>
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

// --- native stylus stream suppression ---------------------------------------
// Qt's iOS platform plugin (quiview.mm) also delivers Apple Pencil contact
// strokes as QTabletEvents. Running both that stream and our bridge synthesis
// duplicates every stroke; and the earlier "hand strokes off to Qt" approach
// produced dots on device: the device log showed the handoff firing, after
// which UIKit gesture interference turned the native stream into
// press+cancel — one dab per stroke, with our synthesis permanently silent.
//
// So the bridge synthesis is the single authoritative stroke source (it is
// also where palm rejection, hover and the double-tap live), and Qt's native
// stylus events are CONSUMED here so they never reach Krita. If the native
// stream doesn't exist in a given Qt build, this filter simply never matches.

class NativeStylusSuppressor : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletMove:
        case QEvent::TabletRelease:
        case QEvent::TabletEnterProximity:
        case QEvent::TabletLeaveProximity: {
            const QTabletEvent *te = static_cast<QTabletEvent *>(event);
            if (te->pointingDevice() != iosStylusDevice()
                && te->deviceType() == QInputDevice::DeviceType::Stylus) {
                static bool logged = false;
                if (!logged) {
                    logged = true;
                    KisUsageLogger::log("Pencil: suppressing Qt-native stylus events (bridge owns the stroke stream)");
                }
                return true; // consume: the bridge synthesis is the only source
            }
            break;
        }
        default:
            break;
        }
        return false;
    }
};

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

namespace
{
bool tryInstallBridge(QWidget *canvas);

// At addCanvas() time the canvas window's native UIView often does not exist
// yet, so the bridge install no-ops (documented in kis_input_manager_p.cpp) —
// on device that left the WHOLE Pencil pipeline dead: no pressure, no palm
// rejection, no double-tap, no hover; drawing fell back to synthesized mouse
// events. Park this filter on the canvas and its window, and retry the install
// as soon as the window is realised (first Show/WinIdChange/Paint).
class DeferredBridgeInstaller : public QObject
{
public:
    explicit DeferredBridgeInstaller(QWidget *canvas)
        : QObject(canvas)
        , m_canvas(canvas)
    {
        canvas->installEventFilter(this);
        if (QWidget *w = canvas->window()) {
            w->installEventFilter(this);
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::WinIdChange:
        case QEvent::Paint:
            if (!m_canvas) {
                deleteLater();
            } else if (tryInstallBridge(m_canvas)) {
                KisUsageLogger::log("Pencil bridge: attached on retry (window realised)");
                m_canvas->setProperty("kisIOSBridgeRetryPending", QVariant());
                deleteLater();
            }
            break;
        default:
            break;
        }
        return false; // observe only
    }

private:
    QPointer<QWidget> m_canvas;
};
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
        // Eat Qt's native stylus stream: the bridge synthesis owns strokes.
        qApp->installEventFilter(new NativeStylusSuppressor(qApp));
    }

    if (tryInstallBridge(canvas)) {
        KisUsageLogger::log("Pencil bridge: attached immediately at addCanvas");
    } else {
        // Window not realised yet — retry on first show (once per canvas).
        KisUsageLogger::log("Pencil bridge: window not realised at addCanvas -> deferred retry armed");
        if (!canvas->property("kisIOSBridgeRetryPending").toBool()) {
            canvas->setProperty("kisIOSBridgeRetryPending", true);
            new DeferredBridgeInstaller(canvas);
        }
    }
}

namespace
{
bool tryInstallBridge(QWidget *canvas)
{
    QPointer<QWidget> target(canvas);

    return KisIOSTabletBridge::install(canvas, [target](const KisIOSPenSample &s) {
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
        // canvas widget's local space, preserving the sub-pixel fraction —
        // integer-rounded positions turn slow diagonal strokes into staircases.
        QWidget *window = target->window();
        const QPointF windowPosF(s.x, s.y);
        const QPoint windowPosI(qRound(s.x), qRound(s.y));
        const QPointF fraction = windowPosF - QPointF(windowPosI);
        const QPointF local = QPointF(target->mapFrom(window, windowPosI)) + fraction;
        const QPointF global = QPointF(window->mapToGlobal(windowPosI)) + fraction;

        // One-shot trace so the device log proves synthesized events flow.
        static bool firstSynthLogged = false;
        if (!firstSynthLogged && s.phase == KisIOSPenSample::Begin) {
            firstSynthLogged = true;
            KisUsageLogger::log(QString("Pencil: first synthesized tablet event (pressure %1)").arg(s.pressure));
        }

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
} // namespace
