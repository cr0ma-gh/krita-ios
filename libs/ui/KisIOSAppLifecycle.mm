/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisIOSAppLifecycle.h"

#include <utility>

#include <QtGlobal>

#import <UIKit/UIKit.h>

namespace
{
// Lifecycle is app-global, so single handlers are stored file-scope.
KisIOSAppLifecycle::Handler g_onBackground;
KisIOSAppLifecycle::Handler g_onMemoryWarning;
} // namespace

@interface KisIOSLifecycleObserver : NSObject
@end

@implementation KisIOSLifecycleObserver

- (void)didEnterBackground:(NSNotification *)note
{
    Q_UNUSED(note);
    if (!g_onBackground) {
        return;
    }

    // iOS freezes the process soon after this notification. Request a short
    // background-task window so the synchronous emergency save can finish
    // writing before we are suspended; end it as soon as the save returns.
    UIApplication *app = [UIApplication sharedApplication];
    __block UIBackgroundTaskIdentifier task =
        [app beginBackgroundTaskWithName:@"KritaAutosave"
                       expirationHandler:^{
                           if (task != UIBackgroundTaskInvalid) {
                               [app endBackgroundTask:task];
                               task = UIBackgroundTaskInvalid;
                           }
                       }];

    g_onBackground();   // synchronous; runs on the main (Qt GUI) thread

    if (task != UIBackgroundTaskInvalid) {
        [app endBackgroundTask:task];
        task = UIBackgroundTaskInvalid;
    }
}

- (void)didReceiveMemoryWarning:(NSNotification *)note
{
    Q_UNUSED(note);
    if (g_onMemoryWarning) {
        g_onMemoryWarning();
    }
}

@end

// NSNotificationCenter does not retain observers, so keep a strong reference for
// the lifetime of the app.
static KisIOSLifecycleObserver *g_lifecycleObserver = nil;

void KisIOSAppLifecycle::install(Handler onBackground, Handler onMemoryWarning)
{
    g_onBackground = std::move(onBackground);
    g_onMemoryWarning = std::move(onMemoryWarning);

    if (g_lifecycleObserver) {
        return;
    }
    g_lifecycleObserver = [[KisIOSLifecycleObserver alloc] init];

    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:g_lifecycleObserver
           selector:@selector(didEnterBackground:)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];
    [nc addObserver:g_lifecycleObserver
           selector:@selector(didReceiveMemoryWarning:)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
}
