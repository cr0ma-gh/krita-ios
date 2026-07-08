/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisIOSTabletBridge.h"

#include <cmath>
#include <utility>

#include <QHash>
#include <QWidget>
#include <QWindow>

#include <KisUsageLogger.h>

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

namespace
{
// Sinks live on the C++ side, keyed by the native view we attached to.
QHash<void *, KisIOSTabletBridge::Sink> g_sinks;

// The Apple Pencil double-tap is a global gesture (a property of the Pencil,
// not of any one view), so a single app-wide handler is stored.
KisIOSTabletBridge::TapSink g_tapSink;

// Our hover recognizer per native view (value is the UIHoverGestureRecognizer,
// owned by the view). Lets install() avoid stacking duplicates and remove()
// detach exactly ours without touching any recognizer Qt may have added.
QHash<void *, void *> g_hoverGRs;
} // namespace

// Tells system Scribble to keep its hands off Pencil strokes on the canvas
// view: without this, iPadOS may capture pencil input for handwriting and
// cancel the app's touch sequence mid-stroke.
API_AVAILABLE(ios(14.0))
@interface KisScribbleBlocker : NSObject <UIScribbleInteractionDelegate>
@end

@implementation KisScribbleBlocker
- (BOOL)scribbleInteraction:(UIScribbleInteraction *)interaction
       shouldBeginAtLocation:(CGPoint)location
{
    Q_UNUSED(interaction);
    Q_UNUSED(location);
    return NO;
}
@end

static KisScribbleBlocker *g_scribbleBlocker = nil;

namespace
{
// iPadOS screen-edge system gestures (home indicator, multitasking) claim a
// touch sequence retroactively and CANCEL it for the app — a stroke or a palm
// starting near an edge dies mid-way. Qt's root view controller does not
// override preferredScreenEdgesDeferringSystemGestures, so patch it at runtime
// to defer all edges (the system then requires a second, deliberate swipe).
void hardenRootViewController(UIView *view)
{
    static bool done = false;
    if (done) {
        return;
    }
    UIViewController *vc = view.window.rootViewController;
    if (!vc) {
        return; // window not up yet; retried on the next touch
    }
    done = true;

    Class cls = object_getClass(vc);
    SEL sel = @selector(preferredScreenEdgesDeferringSystemGestures);
    Method proto = class_getInstanceMethod([UIViewController class], sel);
    IMP imp = imp_implementationWithBlock(^UIRectEdge(id vcSelf) {
        Q_UNUSED(vcSelf);
        return UIRectEdgeAll;
    });
    class_replaceMethod(cls, sel, imp, method_getTypeEncoding(proto));
    [vc setNeedsUpdateOfScreenEdgesDeferringSystemGestures];
    KisUsageLogger::log("Pencil bridge: system screen-edge gestures deferred (root VC hardened)");
}

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

// Translate the user's iOS Settings > Apple Pencil choice
// (UIPencilPreferredAction) into the portable enum the C++ handler consumes, so
// the double-tap behaves exactly as the user configured it system-wide.
KisIOSPencilTapAction mapPreferredAction(UIPencilPreferredAction action)
{
    switch (action) {
    case UIPencilPreferredActionIgnore:
        return KisIOSPencilTapAction::Ignore;
    case UIPencilPreferredActionSwitchEraser:
        return KisIOSPencilTapAction::ToggleEraser;
    case UIPencilPreferredActionSwitchPrevious:
        return KisIOSPencilTapAction::SwitchPrevious;
    case UIPencilPreferredActionShowColorPalette:   // iOS 14+
        return KisIOSPencilTapAction::ShowColorPalette;
    default:
        break;
    }
    // Introduced after our 16.0 deployment target — guard the symbol reference.
    if (@available(iOS 17.5, *)) {
        if (action == UIPencilPreferredActionShowInkAttributes) {
            return KisIOSPencilTapAction::ShowInkAttributes;
        }
    }
    return KisIOSPencilTapAction::Unknown;
}
} // namespace

// Captures the stroke stream. For finger-only sequences it stays in .possible
// and never interferes (Qt receives them for pan/zoom gestures). When a Pencil
// touch begins it CLAIMS the gesture (-> Began/Changed): winning the gesture
// arena forces every competing recognizer on the view (e.g. Qt's long-press
// recognizers) to fail, so none of them can recognise mid-stroke and cancel
// the touch sequence — which showed up on device as strokes that "closed by
// themselves". cancelsTouchesInView stays NO, so claiming still does not steal
// the raw touches from Qt (its native stylus stream is suppressed separately).
@interface KisPencilGestureRecognizer : UIGestureRecognizer
@property (nonatomic, assign) UIView *targetView;
@property (nonatomic, assign) UITouch *activePencil;
@end

@implementation KisPencilGestureRecognizer

- (void)emitTouches:(NSSet<UITouch *> *)touches
              event:(UIEvent *)event
              phase:(KisIOSPenSample::Phase)phase
{
    // One-shot trace: proves UIKit delivers touches to our recognizer at all,
    // and whether the Pencil is recognised as UITouchTypePencil.
    static bool firstTouchLogged = false;
    if (!firstTouchLogged && phase == KisIOSPenSample::Begin) {
        firstTouchLogged = true;
        UITouch *any = touches.anyObject;
        KisUsageLogger::log(QString("Pencil bridge: first UITouch received (type=%1)")
                                .arg(any.type == UITouchTypePencil ? "pencil" : "finger/other"));
    }

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
    // Lazily (the window is guaranteed live here) defer system edge gestures.
    hardenRootViewController(self.targetView);

    [self emitTouches:touches event:event phase:KisIOSPenSample::Begin];

    // Claim the gesture for the Pencil. NEVER use .failed here: the palm
    // usually rests on the glass BEFORE the pen lands, and a recognizer that
    // failed on the palm sits out the whole touch sequence — losing the
    // stroke. Finger-only sequences simply stay in .possible.
    if (!self.activePencil) {
        for (UITouch *touch in touches) {
            if (touch.type == UITouchTypePencil) {
                self.activePencil = touch;
                if (self.state == UIGestureRecognizerStatePossible) {
                    self.state = UIGestureRecognizerStateBegan;
                }
                break;
            }
        }
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self emitTouches:touches event:event phase:KisIOSPenSample::Move];
    if (self.state == UIGestureRecognizerStateBegan
        || self.state == UIGestureRecognizerStateChanged) {
        self.state = UIGestureRecognizerStateChanged;
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self emitTouches:touches event:event phase:KisIOSPenSample::End];
    if (self.activePencil && [touches containsObject:self.activePencil]) {
        self.activePencil = nil;
        if (self.state == UIGestureRecognizerStateBegan
            || self.state == UIGestureRecognizerStateChanged) {
            self.state = UIGestureRecognizerStateEnded;
        }
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self emitTouches:touches event:event phase:KisIOSPenSample::Cancel];

    // Diagnostic: state tells us WHO cancelled. state=Began/Changed (2/3) =
    // we had claimed and a SYSTEM gate overrode us; state=Possible (0) = an
    // app-level recognizer won the arena before our claim.
    static int cancelLogs = 0;
    if (cancelLogs < 5) {
        ++cancelLogs;
        bool hadPencil = false;
        for (UITouch *touch in touches) {
            if (touch.type == UITouchTypePencil) {
                hadPencil = true;
                break;
            }
        }
        KisUsageLogger::log(QString("Pencil bridge: touch sequence CANCELLED by UIKit (state=%1 pencil=%2 touches=%3)")
                                .arg(int(self.state)).arg(hadPencil).arg(int(touches.count)));
    }

    if (self.activePencil && [touches containsObject:self.activePencil]) {
        self.activePencil = nil;
    }
    if (self.state == UIGestureRecognizerStateBegan
        || self.state == UIGestureRecognizerStateChanged) {
        self.state = UIGestureRecognizerStateCancelled;
    }
}

- (void)reset
{
    self.activePencil = nil;
    [super reset];
}

@end

// Receives the Apple Pencil double-tap. UIPencilInteraction.delegate is a *weak*
// property, so a strong reference is held in g_pencilDelegate (below) for the
// lifetime of the app.
@interface KisPencilInteractionDelegate : NSObject <UIPencilInteractionDelegate>
@end

@implementation KisPencilInteractionDelegate
- (void)pencilInteractionDidTap:(UIPencilInteraction *)interaction
{
    Q_UNUSED(interaction);
    KisUsageLogger::log("Pencil: double-tap received from UIPencilInteraction");
    if (!g_tapSink) {
        return;
    }
    // preferredTapAction reflects the live system setting; read it per tap so a
    // mid-session change in Settings is honoured without reinstalling anything.
    g_tapSink(mapPreferredAction(UIPencilInteraction.preferredTapAction));
}
@end

// One shared delegate for the whole app (the interaction only holds it weakly).
static KisPencilInteractionDelegate *g_pencilDelegate = nil;

// Target for the Apple Pencil hover recognizer (iPad Pro M2+, iPadOS 16.1+). It
// emits Hover samples so Krita tracks the live brush outline while the pen is in
// proximity but not touching. Only the Pencil hovers — fingers never do.
API_AVAILABLE(ios(16.1))
@interface KisPencilHoverHandler : NSObject
@property (nonatomic, assign) UIView *targetView;
- (void)hover:(UIHoverGestureRecognizer *)recognizer;
@end

@implementation KisPencilHoverHandler
- (void)hover:(UIHoverGestureRecognizer *)recognizer
{
    auto it = g_sinks.find((__bridge void *)self.targetView);
    if (it == g_sinks.end()) {
        return;
    }

    static bool firstHoverLogged = false;
    if (!firstHoverLogged) {
        firstHoverLogged = true;
        KisUsageLogger::log("Pencil: first hover event from UIHoverGestureRecognizer");
    }

    KisIOSPenSample s;
    s.isPencil = true;   // hover is Pencil-only
    s.pressure = 0.0;    // not in contact
    switch (recognizer.state) {
    case UIGestureRecognizerStateBegan:
    case UIGestureRecognizerStateChanged:
        s.phase = KisIOSPenSample::Hover;
        break;
    default:
        // Ended/Cancelled: the pen left proximity. Emit nothing rather than a
        // spurious release (there was no contact); the outline clears on the
        // next real event. Clean proximity-leave is a documented follow-up.
        return;
    }

    const CGPoint p = [recognizer locationInView:self.targetView];
    s.x = p.x;
    s.y = p.y;

    // The hover recognizer only exposes pencil tilt from iOS 16.4.
    if (@available(iOS 16.4, *)) {
        const double tiltFromVertical = (M_PI / 2.0) - recognizer.altitudeAngle;
        const double azimuth = [recognizer azimuthAngleInView:self.targetView];
        s.tiltX = tiltFromVertical * std::cos(azimuth) * 180.0 / M_PI;
        s.tiltY = tiltFromVertical * std::sin(azimuth) * 180.0 / M_PI;
    }

    it.value()(s);
}
@end

// Retains the hover handlers for the app's lifetime (the recognizer references
// its target strongly, but keeping our own ref is robust under MRC and ARC).
static NSMutableArray *g_pencilHoverHandlers = nil;

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

bool KisIOSTabletBridge::install(QWidget *canvas, Sink sink)
{
    UIView *view = nativeViewFor(canvas);
    if (!view) {
        // Not realised yet (no window / no winId): the caller retries on show.
        return false;
    }

    // Always refresh the sink; only attach recognizers/interactions once per
    // view, so a retry or canvas switch never stacks duplicates.
    const bool alreadyAttached = g_sinks.contains((__bridge void *)view);
    g_sinks.insert((__bridge void *)view, std::move(sink));
    if (alreadyAttached) {
        return true;
    }

    KisUsageLogger::log(QString("Pencil bridge: attaching recognizers to UIView %1")
                            .arg((quintptr)view, 0, 16));

    KisPencilGestureRecognizer *gr = [[KisPencilGestureRecognizer alloc] init];
    gr.targetView = view;
    gr.cancelsTouchesInView = NO;   // do not steal touches from Qt
    gr.delaysTouchesBegan = NO;
    gr.delaysTouchesEnded = NO;
    [view addGestureRecognizer:gr];

    // Apple Pencil double-tap (2nd-gen / Pro): attach the shared delegate so the
    // tap reaches g_tapSink with the user's configured action. Without a delegate
    // the interaction is inert — this is what makes the gesture actually work.
    if (@available(iOS 12.1, *)) {
        if (!g_pencilDelegate) {
            g_pencilDelegate = [[KisPencilInteractionDelegate alloc] init];
        }
        UIPencilInteraction *pencil = [[UIPencilInteraction alloc] init];
        pencil.delegate = g_pencilDelegate;
        [view addInteraction:pencil];
    }

    // Opt the canvas out of system Scribble, which otherwise may capture
    // Pencil strokes for handwriting and cancel the app's touch sequence.
    if (@available(iOS 14.0, *)) {
        if (!g_scribbleBlocker) {
            g_scribbleBlocker = [[KisScribbleBlocker alloc] init];
        }
        UIScribbleInteraction *scribble =
            [[UIScribbleInteraction alloc] initWithDelegate:g_scribbleBlocker];
        [view addInteraction:scribble];
        KisUsageLogger::log("Pencil bridge: Scribble opted out on canvas view");
    }

    // Apple Pencil hover (iPad Pro M2+): track the pen in proximity so the brush
    // outline follows it before contact. Guard against stacking a second
    // recognizer if install() runs again for the same view.
    if (@available(iOS 16.1, *)) {
        if (!g_hoverGRs.contains((__bridge void *)view)) {
            KisPencilHoverHandler *handler = [[KisPencilHoverHandler alloc] init];
            handler.targetView = view;
            if (!g_pencilHoverHandlers) {
                g_pencilHoverHandlers = [[NSMutableArray alloc] init];
            }
            [g_pencilHoverHandlers addObject:handler];

            UIHoverGestureRecognizer *hover =
                [[UIHoverGestureRecognizer alloc] initWithTarget:handler
                                                          action:@selector(hover:)];
            // CRITICAL: a recognizer in Began/Changed cancels the view's
            // touches by default — and the hover recognizer IS in that state
            // whenever the Pencil hovers, i.e. right before every contact. With
            // the default, each stroke's touches were cancelled for the QUIView
            // (Qt saw press+cancel -> a dot). Observe only, never interfere.
            hover.cancelsTouchesInView = NO;
            hover.delaysTouchesBegan = NO;
            hover.delaysTouchesEnded = NO;
            [view addGestureRecognizer:hover];
            g_hoverGRs.insert((__bridge void *)view, (__bridge void *)hover);
        }
    }

    return true;
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

    // Detach exactly our hover recognizer (not any Qt may own).
    auto hit = g_hoverGRs.find((__bridge void *)view);
    if (hit != g_hoverGRs.end()) {
        if (@available(iOS 16.1, *)) {
            UIHoverGestureRecognizer *hover =
                (__bridge UIHoverGestureRecognizer *)hit.value();
            [view removeGestureRecognizer:hover];
        }
        g_hoverGRs.erase(hit);
    }
}

void KisIOSTabletBridge::setPencilTapSink(TapSink sink)
{
    g_tapSink = std::move(sink);
}
