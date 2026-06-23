/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __KISIOSFILEPROXY_H_
#define __KISIOSFILEPROXY_H_

#include <QByteArray>
#include <QString>

#include <kritaui_export.h>

/**
 * iOS counterpart of KisAndroidFileProxy.
 *
 * On iOS the app is sandboxed: files chosen through a UIDocumentPicker arrive as
 * *security-scoped* URLs that path-based code (most of Krita's import/export)
 * cannot open directly. This helper copies such a file into the app sandbox so
 * the rest of Krita can work with a plain local path, mirroring how the Android
 * proxy clones content:// URIs to internal storage. It also offers
 * security-scoped bookmarks so a document can be reopened in place later.
 */
class KRITAUI_EXPORT KisIOSFileProxy
{
public:
    /**
     * Copy a (possibly security-scoped) file URL into the app's writable
     * Documents/Imported directory and return the local filesystem path.
     * Returns an empty string on failure.
     *
     * Counterpart of KisAndroidFileProxy::getFileFromContentUri().
     */
    static QString getFileFromUrl(const QString &fileUrl);

    /**
     * Create a base64 security-scoped bookmark for @p fileUrl, suitable for
     * persisting in the recent-files list so the file can be reopened in place.
     */
    static QByteArray bookmarkForUrl(const QString &fileUrl);

    /**
     * Resolve a bookmark previously produced by bookmarkForUrl() back into a
     * file URL. If @p stale is non-null it is set to true when the bookmark
     * needs to be recreated.
     */
    static QString urlFromBookmark(const QByteArray &bookmark, bool *stale = nullptr);
};

#endif // __KISIOSFILEPROXY_H_
