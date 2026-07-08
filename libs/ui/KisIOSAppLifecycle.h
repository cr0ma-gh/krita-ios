/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISIOSAPPLIFECYCLE_H
#define KISIOSAPPLIFECYCLE_H

#include <functional>

#include <kritaui_export.h>

/**
 * Bridges iOS UIApplication lifecycle notifications to Krita.
 *
 * iPadOS suspends and may terminate memory-hungry apps at any time, and delivers
 * a memory-pressure warning shortly before doing so. This shim observes those
 * events and forwards them to plain-C++ handlers, kept out of this Objective-C++
 * boundary exactly as with KisIOSTabletBridge:
 *
 *  - onBackground: the app is being backgrounded/suspended. The handler should
 *    synchronously persist unsaved work (Krita: KisDocument::autoSaveOnPause()).
 *    install() wraps the call in a UIKit background-task request so iOS grants a
 *    short window to finish writing before freezing the process.
 *  - onMemoryWarning: the system is under memory pressure. The handler should
 *    release memory now (Krita: swap image tiles to disk) to avoid being killed.
 *
 * Both handlers run on the main thread, which is Qt's GUI thread on iOS.
 */
namespace KisIOSAppLifecycle
{
using Handler = std::function<void()>;

/**
 * Start observing UIApplication lifecycle notifications. Idempotent: a second
 * call only updates the handlers (the observer is registered once).
 */
KRITAUI_EXPORT void install(Handler onBackground, Handler onMemoryWarning);
} // namespace KisIOSAppLifecycle

#endif // KISIOSAPPLIFECYCLE_H
