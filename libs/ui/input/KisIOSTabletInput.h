/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISIOSTABLETINPUT_H
#define KISIOSTABLETINPUT_H

#include <kritaui_export.h>

class QWidget;

/**
 * Wires the Apple Pencil capture (KisIOSTabletBridge) to Qt by synthesising
 * QTabletEvents and delivering them to the canvas, which KisInputManager
 * already understands (TabletPress / TabletMove / TabletRelease).
 *
 * Phase 3 scaffolding: the QTabletEvent synthesis lives here, in plain C++,
 * kept out of the Objective-C++ bridge. Call install() once the canvas widget
 * exists and is shown (its native UIView must be live), e.g. from the canvas
 * setup in KisCanvas2 / KisView.
 */
namespace KisIOSTabletInput
{
/// Attach pencil-to-QTabletEvent forwarding to @p canvas.
KRITAUI_EXPORT void install(QWidget *canvas);
} // namespace KisIOSTabletInput

#endif // KISIOSTABLETINPUT_H
