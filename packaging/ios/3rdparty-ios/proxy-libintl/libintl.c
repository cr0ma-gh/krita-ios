/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Stub libintl for iOS: no-op gettext implementations so KI18n (and anything
 * else expecting libintl) links. Messages are returned untranslated — KI18n's
 * own catalog loading still applies on top, but the C-level gettext lookup is a
 * pass-through. Replace with real GNU gettext / proxy-libintl if on-device
 * gettext-based translations are needed.
 */
#include "libintl.h"

/* KI18n increments/reads this to detect catalog changes; a constant is fine. */
int _nl_msg_cat_cntr = 0;

char *gettext(const char *msgid)
{
    return (char *)msgid;
}

char *dgettext(const char *domainname, const char *msgid)
{
    (void)domainname;
    return (char *)msgid;
}

char *dcgettext(const char *domainname, const char *msgid, int category)
{
    (void)domainname;
    (void)category;
    return (char *)msgid;
}

char *ngettext(const char *msgid1, const char *msgid2, unsigned long int n)
{
    return (char *)(n == 1 ? msgid1 : msgid2);
}

char *dngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n)
{
    (void)domainname;
    return (char *)(n == 1 ? msgid1 : msgid2);
}

char *dcngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n, int category)
{
    (void)domainname;
    (void)category;
    return (char *)(n == 1 ? msgid1 : msgid2);
}

char *textdomain(const char *domainname)
{
    return (char *)(domainname ? domainname : "messages");
}

char *bindtextdomain(const char *domainname, const char *dirname)
{
    (void)domainname;
    return (char *)dirname;
}

char *bind_textdomain_codeset(const char *domainname, const char *codeset)
{
    (void)domainname;
    return (char *)codeset;
}
