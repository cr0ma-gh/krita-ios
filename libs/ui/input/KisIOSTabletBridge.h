/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISIOSTABLETBRIDGE_H
#define KISIOSTABLETBRIDGE_H

#include <functional>

#include <kritaui_export.h>

class QWidget;

/**
 * One normalised Apple Pencil / touch sample, in the canvas window's
 * device-independent coordinates.
 */
struct KisIOSPenSample {
    /// Begin/Move/End/Cancel are contact phases (pen touching). Hover is the
    /// pen in proximity but not touching (Apple Pencil hover, iPad Pro M2+).
    enum Phase { Begin, Move, End, Cancel, Hover };

    Phase phase = Move;
    double x = 0.0;
    double y = 0.0;
    double pressure = 0.0;   ///< 0..1 (force / maximumPossibleForce); 0 while hovering
    double tiltX = 0.0;      ///< degrees, -90..90
    double tiltY = 0.0;      ///< degrees, -90..90
    bool isPencil = false;   ///< false => finger
};

/**
 * The action the user assigned to the Apple Pencil double-tap in
 * iOS Settings > Apple Pencil. Mirrors UIKit's UIPencilPreferredAction so the
 * C++ side can honour the *system* preference instead of forcing one behaviour.
 * The signature gesture of the 2nd-generation Apple Pencil (and Pencil Pro).
 */
enum class KisIOSPencilTapAction {
    Ignore,            ///< UIPencilPreferredActionIgnore — user disabled it.
    ToggleEraser,      ///< UIPencilPreferredActionSwitchEraser (Apple default).
    SwitchPrevious,    ///< UIPencilPreferredActionSwitchPrevious tool/preset.
    ShowColorPalette,  ///< UIPencilPreferredActionShowColorPalette (iOS 14+).
    ShowInkAttributes, ///< UIPencilPreferredActionShowInkAttributes (iOS 17.5+).
    Unknown            ///< Newer/unreadable preference — caller picks a default.
};

/**
 * Apple Pencil capture bridge (Phase 3 scaffolding — see README.ios.md §7).
 *
 * iOS delivers stylus input as UITouch carrying .force, .altitudeAngle and
 * .azimuthAngle. This bridge attaches a *non-consuming* UIGestureRecognizer to
 * the canvas window's native UIView, reads the 120 Hz coalesced pencil samples,
 * normalises them and forwards them to @p sink — without stealing the touches
 * from Qt, so ordinary Qt input keeps working.
 *
 * The sink is expected to synthesise QTabletEvents, which KisInputManager
 * already understands (it handles TabletPress / TabletMove / TabletRelease, see
 * libs/ui/input/kis_input_manager.cpp). Keeping that Qt6-specific synthesis on
 * the C++ side (out of this Objective-C++ file) is deliberate: it isolates the
 * version-sensitive QTabletEvent construction from the stable UIKit capture.
 */
namespace KisIOSTabletBridge
{
using Sink = std::function<void(const KisIOSPenSample &)>;

/// Attach pencil capture to @p canvas. Requires the widget's top-level native
/// UIView to exist (the widget must be realised/shown): returns false when it
/// does not yet, so the caller can retry once the window is up. Idempotent per
/// view — repeated calls update the sink without stacking recognizers.
KRITAUI_EXPORT bool install(QWidget *canvas, Sink sink);

/// Detach pencil capture from @p canvas.
KRITAUI_EXPORT void remove(QWidget *canvas);

/**
 * Callback for the Apple Pencil double-tap. Delivered with the action the user
 * chose in iOS Settings, so the handler can act like every other iPad app.
 * Unlike the per-canvas stroke Sink, the tap is app-global (the gesture is a
 * property of the Pencil, not of a particular view), so a single sink is stored.
 * Invoked on the UIKit main thread (= Qt's GUI thread on iOS).
 */
using TapSink = std::function<void(KisIOSPencilTapAction)>;

/// Register the (single, global) Apple Pencil double-tap handler.
KRITAUI_EXPORT void setPencilTapSink(TapSink sink);
} // namespace KisIOSTabletBridge

#endif // KISIOSTABLETBRIDGE_H
