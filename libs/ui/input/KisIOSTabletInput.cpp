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

// --- native-vs-synthesised stroke deduplication -----------------------------
// Qt's iOS platform plugin (quiview.mm) delivers Apple Pencil contact strokes
// natively as QTabletEvents — coalesced samples, precise sub-pixel positions,
// pressure/tilt — when QT_CONFIG(tabletevent) is enabled in the Qt build. If
// both that native stream and our bridge synthesis run, every stroke arrives
// twice (double press/move/release) and the shortcut matcher breaks strokes.
//
// We cannot know at build time whether the deployed Qt delivers the native
// stream, so detect it at runtime: the first native tablet event (a stylus
// event whose device is not ours) permanently hands stroke duplication off to
// Qt, and our synthesis stops emitting contact strokes. If no native event
// ever arrives, our synthesis keeps driving drawing. Hover and the double-tap
// are always ours — Qt's iOS plugin implements neither.

bool g_nativeTabletSeen = false;   // a Qt-native stylus event was observed
bool g_synthStrokeActive = false;  // a synthesised stroke is mid-flight

class NativeTabletDetector : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        if (!g_nativeTabletSeen
            && (event->type() == QEvent::TabletPress || event->type() == QEvent::TabletMove)) {
            const QTabletEvent *te = static_cast<QTabletEvent *>(event);
            if (te->pointingDevice() != iosStylusDevice()
                && te->deviceType() == QInputDevice::DeviceType::Stylus) {
                g_nativeTabletSeen = true;
                KisUsageLogger::log("Pencil: Qt-native stylus events detected -> synthesis handed off to Qt");
            }
        }
        return false; // observe only, never consume
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
        // Watch for Qt-native stylus events so synthesis can hand off to them.
        qApp->installEventFilter(new NativeTabletDetector(qApp));
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

        // Contact strokes: synthesise only while Qt's native tablet stream is
        // absent (see NativeTabletDetector). Strokes are gated at Begin so a
        // stroke started by synthesis is always completed by synthesis, even if
        // the first native event lands mid-stroke.
        if (s.phase == KisIOSPenSample::Begin) {
            if (g_nativeTabletSeen) {
                return;
            }
            g_synthStrokeActive = true;
        } else if (s.phase != KisIOSPenSample::Hover) {
            if (!g_synthStrokeActive) {
                return;
            }
            if (s.phase == KisIOSPenSample::End || s.phase == KisIOSPenSample::Cancel) {
                g_synthStrokeActive = false;
            }
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
