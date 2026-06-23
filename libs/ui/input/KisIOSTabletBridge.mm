/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisIOSTabletBridge.h"

#include <cmath>

#include <QHash>
#include <QWidget>
#include <QWindow>

#import <UIKit/UIKit.h>

namespace
{
// Sinks live on the C++ side, keyed by the native view we attached to.
QHash<void *, KisIOSTabletBridge::Sink> g_sinks;

KisIOSPenSample makeSample(UITouch *touch, UIView *view, KisIOSPenSample::Phase phase)
{
    KisIOSPenSample s;
    s.phase = phase;
    s.isPencil = (touch.type == UITouchTypePencil);

    const CGPoint p = [touch locationInView:view];
    s.x = p.x;
    s.y = p.y;

    if (touch.maximumPossibleForce > 0.0) {
        s.pressure = static_cast<double>(touch.force / touch.maximumPossibleForce);
    } else {
        // Fingers report no force on most devices; treat as full contact.
        s.pressure = s.isPencil ? 0.0 : 1.0;
    }

    if (s.isPencil) {
        // altitudeAngle: 0 = flat on screen, pi/2 = perpendicular.
        // azimuthAngle: direction the pencil points, in view coordinates.
        const double tiltFromVertical = (M_PI / 2.0) - touch.altitudeAngle;
        const double azimuth = [touch azimuthAngleInView:view];
        s.tiltX = tiltFromVertical * std::cos(azimuth) * 180.0 / M_PI;
        s.tiltY = tiltFromVertical * std::sin(azimuth) * 180.0 / M_PI;
    }
    return s;
}
} // namespace

// A gesture recognizer that observes touches without consuming them, so Qt
// still receives the original events.
@interface KisPencilGestureRecognizer : UIGestureRecognizer
@property (nonatomic, assign) UIView *targetView;
@end

@implementation KisPencilGestureRecognizer

- (void)emitTouches:(NSSet<UITouch *> *)touches
              event:(UIEvent *)event
              phase:(KisIOSPenSample::Phase)phase
{
    auto it = g_sinks.find((__bridge void *)self.targetView);
    if (it == g_sinks.end()) {
        return;
    }
    const KisIOSTabletBridge::Sink &sink = it.value();

    for (UITouch *touch in touches) {
        // Coalesced touches give every sample at the display's full (120 Hz)
        // rate, not just one per frame — essential for smooth strokes.
        NSArray<UITouch *> *coalesced = [event coalescedTouchesForTouch:touch];
        if (coalesced.count > 0) {
            for (UITouch *c in coalesced) {
                sink(makeSample(c, self.targetView, phase));
            }
        } else {
            sink(makeSample(touch, self.targetView, phase));
        }
    }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self emitTouches:touches event:event phase:KisIOSPenSample::Begin];
    // Stay in Possible/Failed so we never cancel the touches for Qt.
    self.state = UIGestureRecognizerStateFailed;
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self emitTouches:touches event:event phase:KisIOSPenSample::Move];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self emitTouches:touches event:event phase:KisIOSPenSample::End];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self emitTouches:touches event:event phase:KisIOSPenSample::Cancel];
}

@end

namespace
{
UIView *nativeViewFor(QWidget *canvas)
{
    if (!canvas) {
        return nullptr;
    }
    // The canvas may not be a native widget; attach to its top-level window,
    // whose winId() is the QUIView Qt created for it.
    QWidget *top = canvas->window();
    if (!top || !top->windowHandle()) {
        return nullptr;
    }
    return reinterpret_cast<UIView *>(top->winId());
}
} // namespace

void KisIOSTabletBridge::install(QWidget *canvas, Sink sink)
{
    UIView *view = nativeViewFor(canvas);
    if (!view) {
        return;
    }

    g_sinks.insert((__bridge void *)view, std::move(sink));

    KisPencilGestureRecognizer *gr = [[KisPencilGestureRecognizer alloc] init];
    gr.targetView = view;
    gr.cancelsTouchesInView = NO;   // do not steal touches from Qt
    gr.delaysTouchesBegan = NO;
    gr.delaysTouchesEnded = NO;
    [view addGestureRecognizer:gr];

    // Apple Pencil double-tap / squeeze gesture (e.g. toggle eraser).
    if (@available(iOS 12.1, *)) {
        UIPencilInteraction *pencil = [[UIPencilInteraction alloc] init];
        [view addInteraction:pencil];
    }
}

void KisIOSTabletBridge::remove(QWidget *canvas)
{
    UIView *view = nativeViewFor(canvas);
    if (!view) {
        return;
    }
    g_sinks.remove((__bridge void *)view);
    for (UIGestureRecognizer *gr in [view.gestureRecognizers copy]) {
        if ([gr isKindOfClass:[KisPencilGestureRecognizer class]]) {
            [view removeGestureRecognizer:gr];
        }
    }
}
