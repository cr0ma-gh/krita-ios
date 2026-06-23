/*
 * SPDX-FileCopyrightText: 2026 The Krita iOS port contributors
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisIOSFileProxy.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <kis_debug.h>

#import <Foundation/Foundation.h>

namespace
{
/** Build an NSURL from a Qt URL/path string, tolerating both forms. */
NSURL *nsUrlFromQString(const QString &fileUrl)
{
    NSString *s = fileUrl.toNSString();
    if (fileUrl.contains(QStringLiteral("://"))) {
        return [NSURL URLWithString:s];
    }
    return [NSURL fileURLWithPath:s];
}

/** Directory inside the sandbox where imported copies live. */
QString importDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dir = base + QStringLiteral("/Imported");
    QDir().mkpath(dir);
    return dir;
}
} // namespace

QString KisIOSFileProxy::getFileFromUrl(const QString &fileUrl)
{
    @autoreleasepool {
        NSURL *url = nsUrlFromQString(fileUrl);
        if (!url) {
            warnKrita << "KisIOSFileProxy: could not parse URL" << fileUrl;
            return QString();
        }

        // Security-scoped resources must be opened explicitly and balanced.
        const BOOL scoped = [url startAccessingSecurityScopedResource];

        NSString *name = [url lastPathComponent];
        if (name.length == 0) {
            name = @"imported";
        }
        const QString dest = QDir(importDir()).absoluteFilePath(QString::fromNSString(name));

        // Coordinated read so iCloud / Files providers materialise the file.
        __block BOOL ok = NO;
        __block NSError *coordError = nil;
        NSFileCoordinator *coordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
        [coordinator coordinateReadingItemAtURL:url
                                        options:NSFileCoordinatorReadingWithoutChanges
                                          error:&coordError
                                     byAccessor:^(NSURL *readURL) {
            NSFileManager *fm = [NSFileManager defaultManager];
            NSURL *destURL = [NSURL fileURLWithPath:dest.toNSString()];
            [fm removeItemAtURL:destURL error:nil];
            NSError *copyError = nil;
            ok = [fm copyItemAtURL:readURL toURL:destURL error:&copyError];
            if (!ok) {
                NSLog(@"KisIOSFileProxy copy failed: %@", copyError);
            }
        }];

        if (scoped) {
            [url stopAccessingSecurityScopedResource];
        }

        if (!ok || coordError) {
            warnKrita << "KisIOSFileProxy: failed to import" << fileUrl;
            return QString();
        }
        return dest;
    }
}

QByteArray KisIOSFileProxy::bookmarkForUrl(const QString &fileUrl)
{
    @autoreleasepool {
        NSURL *url = nsUrlFromQString(fileUrl);
        const BOOL scoped = [url startAccessingSecurityScopedResource];
        NSError *error = nil;
        NSData *data = [url bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                    includingResourceValuesForKeys:nil
                                     relativeToURL:nil
                                             error:&error];
        if (scoped) {
            [url stopAccessingSecurityScopedResource];
        }
        if (!data || error) {
            warnKrita << "KisIOSFileProxy: bookmark creation failed for" << fileUrl;
            return QByteArray();
        }
        return QByteArray::fromNSData(data).toBase64();
    }
}

QString KisIOSFileProxy::urlFromBookmark(const QByteArray &bookmark, bool *stale)
{
    @autoreleasepool {
        const QByteArray raw = QByteArray::fromBase64(bookmark);
        NSData *data = raw.toNSData();
        BOOL isStale = NO;
        NSError *error = nil;
        NSURL *url = [NSURL URLByResolvingBookmarkData:data
                                               options:0
                                         relativeToURL:nil
                                   bookmarkDataIsStale:&isStale
                                                 error:&error];
        if (stale) {
            *stale = isStale;
        }
        if (!url || error) {
            return QString();
        }
        return QString::fromNSString(url.absoluteString);
    }
}
