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
    enum Phase { Begin, Move, End, Cancel };

    Phase phase = Move;
    double x = 0.0;
    double y = 0.0;
    double pressure = 0.0;   ///< 0..1 (force / maximumPossibleForce)
    double tiltX = 0.0;      ///< degrees, -90..90
    double tiltY = 0.0;      ///< degrees, -90..90
    bool isPencil = false;   ///< false => finger
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

/// Attach pencil capture to @p canvas. Call after the widget is shown/mapped.
KRITAUI_EXPORT void install(QWidget *canvas, Sink sink);

/// Detach pencil capture from @p canvas.
KRITAUI_EXPORT void remove(QWidget *canvas);
} // namespace KisIOSTabletBridge

#endif // KISIOSTABLETBRIDGE_H
