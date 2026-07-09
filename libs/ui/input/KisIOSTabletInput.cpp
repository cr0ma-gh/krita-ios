/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisIOSTabletInput.h"

#include "KisIOSTabletBridge.h"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPoint>
#include <QPointer>
#include <QTabletEvent>
#include <QVariant>
#include <QVector>
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
// duplicates every stroke, so over the CANVAS our synthesis is the single
// stroke source and the native stream is consumed here.
//
// Two hard-won subtleties:
//  * A consumed-but-not-accepted tablet event makes Qt synthesize MOUSE events
//    from it (AA_SynthesizeMouseForUnhandledTabletEvents): a second,
//    pressure-less pointer stream raced our synthesis on device — uniform
//    strokes and broken stroke ends. setAccepted(true) before consuming.
//  * The suppression must apply ONLY over the canvas: outside it (toolbox,
//    dockers) the pen must keep clicking UI, which works exactly through that
//    mouse-from-tablet synthesis. So foreign stylus events over UI pass
//    through untouched.

// Canvas widgets the bridge drives; used to scope suppression to the canvas.
QVector<QPointer<QWidget>> g_canvasWidgets;

// Armed on every synthesized stroke end; the suppressor then logs the next
// pointer events hitting a canvas within 300 ms — to NAME whatever event
// draws the "stroke closes onto itself" artifact reported on device.
QElapsedTimer g_postReleaseWatch;

bool overAnyCanvas(const QPoint &globalPos)
{
    for (const QPointer<QWidget> &w : g_canvasWidgets) {
        if (w && w->isVisible()) {
            const QRect global(w->mapToGlobal(QPoint(0, 0)), w->size());
            if (global.contains(globalPos)) {
                return true;
            }
        }
    }
    return false;
}

class NativeStylusSuppressor : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        // Post-release tracer: name the pointer events reaching a canvas right
        // after a synthesized stroke ends (first 10 lines overall).
        if (g_postReleaseWatch.isValid() && g_postReleaseWatch.elapsed() < 300) {
            const QEvent::Type t = event->type();
            const bool pointerish = (t == QEvent::MouseButtonPress || t == QEvent::MouseMove
                                     || t == QEvent::MouseButtonRelease || t == QEvent::TabletPress
                                     || t == QEvent::TabletMove || t == QEvent::TouchBegin
                                     || t == QEvent::TouchUpdate);
            if (pointerish) {
                QWidget *w = qobject_cast<QWidget *>(watched);
                bool isCanvas = false;
                if (w) {
                    for (const QPointer<QWidget> &cw : g_canvasWidgets) {
                        if (cw == w) {
                            isCanvas = true;
                            break;
                        }
                    }
                }
                if (isCanvas) {
                    static int traced = 0;
                    if (traced < 10) {
                        ++traced;
                        QString detail;
                        if (t == QEvent::MouseButtonPress || t == QEvent::MouseMove
                            || t == QEvent::MouseButtonRelease) {
                            const QMouseEvent *me = static_cast<QMouseEvent *>(event);
                            detail = QString("pos %1,%2 source %3")
                                         .arg(me->position().x()).arg(me->position().y())
                                         .arg(int(me->source()));
                        } else if (t == QEvent::TabletPress || t == QEvent::TabletMove) {
                            const QTabletEvent *tev = static_cast<QTabletEvent *>(event);
                            detail = QString("pos %1,%2 device %3")
                                         .arg(tev->position().x()).arg(tev->position().y())
                                         .arg(tev->pointingDevice() == iosStylusDevice() ? "ours" : "foreign");
                        }
                        KisUsageLogger::log(QString("Pencil: post-release event on canvas: type %1 %2 (+%3ms)")
                                                .arg(int(t)).arg(detail).arg(g_postReleaseWatch.elapsed()));
                    }
                }
            }
        }

        switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletMove:
        case QEvent::TabletRelease: {
            QTabletEvent *te = static_cast<QTabletEvent *>(event);
            if (te->pointingDevice() != iosStylusDevice()
                && te->deviceType() == QInputDevice::DeviceType::Stylus) {
                if (!overAnyCanvas(te->globalPosition().toPoint())) {
                    // Pen over UI: let Qt's mouse synthesis click widgets.
                    return false;
                }
                static bool logged = false;
                if (!logged) {
                    logged = true;
                    KisUsageLogger::log("Pencil: suppressing Qt-native stylus events over the canvas");
                }
                // Accepted + consumed: no duplicate stroke AND no synthesized
                // mouse stream from it.
                te->setAccepted(true);
                return true;
            }
            break;
        }
        case QEvent::TabletEnterProximity:
        case QEvent::TabletLeaveProximity: {
            const QTabletEvent *te = static_cast<QTabletEvent *>(event);
            if (te->pointingDevice() != iosStylusDevice()
                && te->deviceType() == QInputDevice::DeviceType::Stylus) {
                return true; // no positions to scope by; harmless to drop
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
// Per-stroke bookkeeping for the synthesis (GUI thread only, plain globals).
bool g_strokeInsideCanvas = false;
int g_strokeSamples = 0;
double g_strokeMinPressure = 1.0;
double g_strokeMaxPressure = 0.0;
QPointF g_strokeStartLocal;

bool tryInstallBridge(QWidget *canvas)
{
    QPointer<QWidget> target(canvas);

    const bool ok = KisIOSTabletBridge::install(canvas, [target](const KisIOSPenSample &s) {
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

        // Strokes must START on the canvas. The recognizer sees every pencil
        // touch in the whole window (toolbox, dockers, popups included);
        // forwarding those to the canvas as tablet presses injected spurious
        // strokes at nonsense coordinates and corrupted running ones. A stroke
        // that began inside may wander outside (dragging past the edge is
        // normal); hover is simply clipped to the canvas.
        if (s.phase == KisIOSPenSample::Begin) {
            g_strokeInsideCanvas = target->rect().contains(local.toPoint());
            if (!g_strokeInsideCanvas) {
                return;
            }
            g_strokeSamples = 1;
            g_strokeMinPressure = s.pressure;
            g_strokeMaxPressure = s.pressure;
            g_strokeStartLocal = local;
        } else if (s.phase == KisIOSPenSample::Hover) {
            if (!target->rect().contains(local.toPoint())) {
                return;
            }
        } else {
            if (!g_strokeInsideCanvas) {
                return;
            }
            ++g_strokeSamples;
            g_strokeMinPressure = qMin(g_strokeMinPressure, s.pressure);
            g_strokeMaxPressure = qMax(g_strokeMaxPressure, s.pressure);
            if (s.phase == KisIOSPenSample::End || s.phase == KisIOSPenSample::Cancel) {
                g_strokeInsideCanvas = false;
                // Prove on-device whether pressure varies within a stroke.
                static int summaries = 0;
                if (summaries < 5) {
                    ++summaries;
                    KisUsageLogger::log(QString("Pencil: stroke ended (%1): %2 samples, pressure %3..%4, from %5,%6 to %7,%8")
                                            .arg(s.phase == KisIOSPenSample::Cancel ? "cancel" : "end")
                                            .arg(g_strokeSamples)
                                            .arg(g_strokeMinPressure)
                                            .arg(g_strokeMaxPressure)
                                            .arg(g_strokeStartLocal.x()).arg(g_strokeStartLocal.y())
                                            .arg(local.x()).arg(local.y()));
                }
            }
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
        const bool handled = QApplication::sendEvent(target, &ev);

        // Arm the post-release tracer (see NativeStylusSuppressor).
        if (s.phase == KisIOSPenSample::End || s.phase == KisIOSPenSample::Cancel) {
            g_postReleaseWatch.restart();
        }

        // One-shot: prove whether Krita's input manager actually TOOK the
        // synthesized stroke (accepted=0 would mean painting still runs on a
        // fallback pointer stream — the next thing to chase).
        static bool firstSynthLogged = false;
        if (!firstSynthLogged && s.phase == KisIOSPenSample::Begin) {
            firstSynthLogged = true;
            KisUsageLogger::log(QString("Pencil: first synthesized tablet press (pressure %1, handled %2, accepted %3)")
                                    .arg(s.pressure).arg(handled).arg(ev.isAccepted()));
        }
    });

    if (ok) {
        bool known = false;
        for (const QPointer<QWidget> &w : g_canvasWidgets) {
            if (w == canvas) {
                known = true;
                break;
            }
        }
        if (!known) {
            g_canvasWidgets.append(QPointer<QWidget>(canvas));
        }
    }
    return ok;
}
} // namespace
