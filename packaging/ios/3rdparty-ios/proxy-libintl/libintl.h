/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Minimal libintl header for the iOS build. The iOS libc has no gettext and we
 * do not cross-compile full GNU gettext, so KI18n links against the stub in
 * libintl.c. Declares the gettext entry points KI18n's FindLibIntl probes for.
 */
#ifndef KRITA_IOS_LIBINTL_H
#define KRITA_IOS_LIBINTL_H

#ifdef __cplusplus
extern "C" {
#endif

extern int _nl_msg_cat_cntr;

char *gettext(const char *msgid);
char *dgettext(const char *domainname, const char *msgid);
char *dcgettext(const char *domainname, const char *msgid, int category);
char *ngettext(const char *msgid1, const char *msgid2, unsigned long int n);
char *dngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n);
char *dcngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n, int category);
char *textdomain(const char *domainname);
char *bindtextdomain(const char *domainname, const char *dirname);
char *bind_textdomain_codeset(const char *domainname, const char *codeset);

#ifdef __cplusplus
}
#endif

#endif /* KRITA_IOS_LIBINTL_H */
