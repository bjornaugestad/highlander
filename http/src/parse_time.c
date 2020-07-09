/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */


#include <string.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>

#include "internals.h"

/*
 * Creates a time_t based on the input.
 * The input must be in the format described in rfc822, plus
 * the changes from rfc1123 (just the 4-digit year requirement).
 * Returns -1 on error.
 */
time_t parse_rfc822_date(const char *s)
{
    static const char *wkday[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };

    static const char *month[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    size_t i, size;
    int imonth, day_of_month = 0;
    int year = 0;
    int h, m, sec;
    struct tm tm;

    assert(s != NULL);

    /* We are going to read 29 bytes from the pointer, so it must be that many bytes long.	*/
    if (strlen(s) != 29)
        return -1;

    /* Get index for the day */
    size = sizeof wkday / sizeof *wkday;
    for (i = 0; i < size; i++) {
        if (!memcmp(s, wkday[i], 3))
            break;
    }

    /* Not found */
    if (i == size)
        return -1;

    /* Now move to next field which is date as in: 02 SP Jun SP 1982 */
    s += 5;
    assert(isdigit((unsigned char)*s) && isdigit((unsigned char)*(s+1)));
    day_of_month = ((*s - '0') * 10) + (*s - '0');

    /* find month */
    s+= 3;
    assert(isalpha((unsigned char)*s) && isupper((unsigned char)*s));

    imonth = 0;
    size = sizeof month / sizeof *month;
    for (i = 0; i < size; i++) {
        if (!memcmp(s, month[i], 3)) {
            imonth = i;
            break;
        }
    }

    /* Not found */
    if (i == size)
        return -1;


    /* Move to year */
    s += 4;
    year = 0;
    for (i = 0; i < 4; i++) {
        assert(isdigit((unsigned char)s[i]));
        year = (year * 10) + (s[i] - '0');
    }

    /* Move to time and read it. Format is hh:mm:ss */
    s += 5;
    assert(isdigit(*s) && isdigit(*(s+1)));
    h = ((*s - '0') * 10) + (*(s + 1) - '0');

    assert(':' == *(s + 2));
    m = ((*(s + 3) - '0') * 10) + (*(s + 4) - '0');

    assert(':' == *(s + 5));
    sec = ((*(s + 6) - '0') * 10) + (*(s + 7) - '0');

    assert(h < 24);
    assert(m < 60);
    assert(sec < 60);

    /* Now move to the timezone, which must be GMT */
    s += 9; /* 8 for the time and 1 for the separating space */
    if (memcmp(s, "GMT", 3))
        return -1;

    /* That's it, convert it to a time_t */
    tm.tm_sec = sec;
    tm.tm_min = m;
    tm.tm_hour = h;
    tm.tm_mday = day_of_month;
    tm.tm_mon = imonth;
    tm.tm_year = year - 1900;
    tm.tm_wday = 0;
    tm.tm_isdst = 0;
    return mktime(&tm);
}
