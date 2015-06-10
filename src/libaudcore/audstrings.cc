/*
 * audstrings.c
 * Copyright 2009-2012 John Lindgren and William Pitcock
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include "audstrings.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>

#include <glib.h>

#include "i18n.h"
#include "index.h"
#include "internal.h"
#include "runtime.h"

static const char ascii_to_hex[256] =
    "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
    "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
    "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
    "\x0\x1\x2\x3\x4\x5\x6\x7\x8\x9\x0\x0\x0\x0\x0\x0"
    "\x0\xa\xb\xc\xd\xe\xf\x0\x0\x0\x0\x0\x0\x0\x0\x0"
    "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
    "\x0\xa\xb\xc\xd\xe\xf\x0\x0\x0\x0\x0\x0\x0\x0\x0"
    "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0";

static const char hex_to_ascii[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

static const char uri_legal_table[256] =
    "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
    "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
    "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x1\x1\x1"  // '-' '.' '/'
    "\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x0\x0\x0\x0\x0\x0"  // 0-9
    "\x0\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1"  // A-O
    "\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x0\x0\x0\x0\x1"  // P-Z '_'
    "\x0\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1"  // a-o
    "\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1\x0\x0\x0\x1\x0"; // p-z '~'

static const char swap_case[256] =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
    "\0ABCDEFGHIJKLMNOPQRSTUVWXYZ\0\0\0\0\0";

#define FROM_HEX(c)  (ascii_to_hex[(unsigned char) (c)])
#define TO_HEX(i)    (hex_to_ascii[(i) & 15])
#define IS_LEGAL(c)  (uri_legal_table[(unsigned char) (c)])
#define SWAP_CASE(c) (swap_case[(unsigned char) (c)])

/* strcmp() that handles nullptr safely */
EXPORT int strcmp_safe (const char * a, const char * b, int len)
{
    if (! a)
        return b ? -1 : 0;
    if (! b)
        return 1;

    return len < 0 ? strcmp (a, b) : strncmp (a, b, len);
}

/* ASCII version of strcasecmp, also handles nullptr safely */
EXPORT int strcmp_nocase (const char * a, const char * b, int len)
{
    if (! a)
        return b ? -1 : 0;
    if (! b)
        return 1;

    return len < 0 ? g_ascii_strcasecmp (a, b) : g_ascii_strncasecmp (a, b, len);
}

/* strlen() if <len> is negative, otherwise strnlen() */
EXPORT int strlen_bounded (const char * s, int len)
{
    if (len < 0)
        return strlen (s);

    const char * nul = (const char *) memchr (s, 0, len);
    if (nul)
        return nul - s;

    return len;
}

EXPORT StringBuf str_copy (const char * s, int len)
{
    if (len < 0)
        len = strlen (s);

    StringBuf str (len);
    memcpy (str, s, len);
    return str;
}

EXPORT StringBuf str_concat (const std::initializer_list<const char *> & strings)
{
    StringBuf str (-1);
    char * set = str;
    int left = str.len ();

    for (const char * s : strings)
    {
        int len = strlen (s);
        if (len > left)
            throw std::bad_alloc ();

        memcpy (set, s, len);

        set += len;
        left -= len;
    }

    str.resize (set - str);
    return str;
}

EXPORT StringBuf str_printf (const char * format, ...)
{
    va_list args;
    va_start (args, format);
    StringBuf str = str_vprintf (format, args);
    va_end (args);
    return str;
}

EXPORT StringBuf str_vprintf (const char * format, va_list args)
{
    StringBuf str (-1);
    int len = vsnprintf (str, str.len (), format, args);
    str.resize (len);
    return str;
}

EXPORT bool str_has_prefix_nocase (const char * str, const char * prefix)
{
    return ! g_ascii_strncasecmp (str, prefix, strlen (prefix));
}

EXPORT bool str_has_suffix_nocase (const char * str, const char * suffix)
{
    int len1 = strlen (str);
    int len2 = strlen (suffix);

    if (len2 > len1)
        return false;

    return ! g_ascii_strcasecmp (str + len1 - len2, suffix);
}

/* Bernstein's hash function (unrolled version):
 *    h(0) = 5381
 *    h(n) = 33 * h(n-1) + c
 *
 * This function is more than twice as fast as g_str_hash (a simpler version of
 * Bernstein's hash) and even slightly faster than Murmur 3. */

EXPORT unsigned str_calc_hash (const char * s)
{
    unsigned h = 5381;

    int len = strlen (s);

    while (len >= 8)
    {
        h = h * 1954312449 +
            s[0] * 3963737313 +
            s[1] * 1291467969 +
            s[2] * 39135393 +
            s[3] * 1185921 +
            s[4] * 35937 +
            s[5] * 1089 +
            s[6] * 33 +
            s[7];

        s += 8;
        len -= 8;
    }

    if (len >= 4)
    {
        h = h * 1185921 +
            s[0] * 35937 +
            s[1] * 1089 +
            s[2] * 33 +
            s[3];

        s += 4;
        len -= 4;
    }

    switch (len)
    {
        case 3: h = h * 33 + (* s ++);
        case 2: h = h * 33 + (* s ++);
        case 1: h = h * 33 + (* s ++);
    }

    return h;
}

EXPORT const char * strstr_nocase (const char * haystack, const char * needle)
{
    while (1)
    {
        const char * ap = haystack;
        const char * bp = needle;

        while (1)
        {
            char a = * ap ++;
            char b = * bp ++;

            if (! b) /* all of needle matched */
                return (char *) haystack;
            if (! a) /* end of haystack reached */
                return nullptr;

            if (a != b && a != SWAP_CASE (b))
                break;
        }

        haystack ++;
    }
}

EXPORT const char * strstr_nocase_utf8 (const char * haystack, const char * needle)
{
    while (1)
    {
        const char * ap = haystack;
        const char * bp = needle;

        while (1)
        {
            gunichar a = g_utf8_get_char (ap);
            gunichar b = g_utf8_get_char (bp);

            if (! b) /* all of needle matched */
                return (char *) haystack;
            if (! a) /* end of haystack reached */
                return nullptr;

            if (a != b && (a < 128 ? (gunichar) SWAP_CASE (a) != b :
             g_unichar_tolower (a) != g_unichar_tolower (b)))
                break;

            ap = g_utf8_next_char (ap);
            bp = g_utf8_next_char (bp);
        }

        haystack = g_utf8_next_char (haystack);
    }
}

EXPORT StringBuf str_tolower (const char * str)
{
    StringBuf buf (strlen (str));
    char * set = buf;

    while (* str)
        * set ++ = g_ascii_tolower (* str ++);

    return buf;
}

EXPORT StringBuf str_tolower_utf8 (const char * str)
{
    StringBuf buf (6 * strlen (str));
    char * set = buf;
    gunichar c;

    while ((c = g_utf8_get_char (str)))
    {
        if (c < 128)
            * set ++ = g_ascii_tolower (c);
        else
            set += g_unichar_to_utf8 (g_unichar_tolower (c), set);

        str = g_utf8_next_char (str);
    }

    buf.resize (set - buf);
    return buf;
}

EXPORT void str_replace_char (char * string, char old_c, char new_c)
{
    while ((string = strchr (string, old_c)))
        * string ++ = new_c;
}

/* Percent-decodes <len> bytes of <str>.  If <len> is negative, decodes all of <str>. */

EXPORT StringBuf str_decode_percent (const char * str, int len)
{
    if (len < 0)
        len = strlen (str);

    StringBuf buf (len);
    char * out = buf;

    while (1)
    {
        const char * p = (const char *) memchr (str, '%', len);
        if (! p)
            break;

        int block = p - str;
        memcpy (out, str, block);

        str += block;
        out += block;
        len -= block;

        if (len < 3)
            break;

        * out ++ = (FROM_HEX (str[1]) << 4) | FROM_HEX (str[2]);

        str += 3;
        len -= 3;
    }

    memcpy (out, str, len);
    buf.resize (out + len - buf);
    return buf;
}

/* Percent-encodes <len> bytes of <str>.  If <len> is negative, decodes all of <str>. */

EXPORT StringBuf str_encode_percent (const char * str, int len)
{
    if (len < 0)
        len = strlen (str);

    StringBuf buf (3 * len);
    char * out = buf;

    while (len --)
    {
        char c = * str ++;

        if (IS_LEGAL (c))
            * out ++ = c;
        else
        {
            * out ++ = '%';
            * out ++ = TO_HEX ((unsigned char) c >> 4);
            * out ++ = TO_HEX (c & 0xF);
        }
    }

    buf.resize (out - buf);
    return buf;
}

EXPORT StringBuf filename_normalize (StringBuf && filename)
{
    int len;
    char * s;

#ifdef _WIN32
    /* convert slash to backslash on Windows */
    str_replace_char (filename, '/', '\\');
#endif

    /* remove current directory (".") elements */
    while ((len = filename.len ()) >= 2 &&
     (! strcmp ((s = filename + len - 2), G_DIR_SEPARATOR_S ".") ||
     (s = strstr (filename, G_DIR_SEPARATOR_S "." G_DIR_SEPARATOR_S))))
        filename.remove (s + 1 - filename, aud::min (s + 3, filename + len) - (s + 1));

    /* remove parent directory ("..") elements */
    while ((len = filename.len ()) >= 3 &&
     (! strcmp ((s = filename + len - 3), G_DIR_SEPARATOR_S "..") ||
     (s = strstr (filename, G_DIR_SEPARATOR_S ".." G_DIR_SEPARATOR_S))))
    {
        * s = 0;
        char * s2 = strrchr (filename, G_DIR_SEPARATOR);
        if (! s2)
            * (s2 = s) = G_DIR_SEPARATOR;

        filename.remove (s2 + 1 - filename, aud::min (s + 4, filename + len) - (s2 + 1));
    }

    /* remove trailing slash */
#ifdef _WIN32
    if ((len = filename.len ()) > 3 && filename[len - 1] == '\\') /* leave "C:\" */
#else
    if ((len = filename.len ()) > 1 && filename[len - 1] == '/') /* leave leading "/" */
#endif
        filename.resize (len - 1);

    return std::move (filename);
}

EXPORT StringBuf filename_build (const std::initializer_list<const char *> & elems)
{
    StringBuf str (-1);
    char * set = str;
    int left = str.len ();

    for (const char * s : elems)
    {
#ifdef _WIN32
        if (set > str && set[-1] != '/' && set[-1] != '\\')
        {
            if (! left)
                throw std::bad_alloc ();

            * set ++ = '\\';
            left --;
        }
#else
        if (set > str && set[-1] != '/')
        {
            if (! left)
                throw std::bad_alloc ();

            * set ++ = '/';
            left --;
        }
#endif

        int len = strlen (s);
        if (len > left)
            throw std::bad_alloc ();

        memcpy (set, s, len);

        set += len;
        left -= len;
    }

    str.resize (set - str);
    return str;
}

#ifdef _WIN32
#define URI_PREFIX "file:///"
#define URI_PREFIX_LEN 8
#else
#define URI_PREFIX "file://"
#define URI_PREFIX_LEN 7
#endif

/* Like g_filename_to_uri, but converts the filename from the system locale to
 * UTF-8 before percent-encoding (except on Windows, where filenames are assumed
 * to be UTF-8).  On Windows, replaces '\' with '/' and adds a leading '/'. */

EXPORT StringBuf filename_to_uri (const char * name)
{
#ifdef _WIN32
    StringBuf buf = str_copy (name);
    str_replace_char (buf, '\\', '/');
#else
    StringBuf buf;

    /* convert from locale if:
     * 1) system locale is not UTF-8, and
     * 2) filename is not already valid UTF-8 */
    if (! g_get_charset (nullptr) && ! g_utf8_validate (name, -1, nullptr))
        buf.steal (str_from_locale (name));

    if (! buf)
        buf.steal (str_copy (name));
#endif

    buf.steal (str_encode_percent (buf));
    buf.insert (0, URI_PREFIX);
    return buf;
}

/* Like g_filename_from_uri, but converts the filename from UTF-8 to the system
 * locale after percent-decoding (except on Windows, where filenames are assumed
 * to be UTF-8).  On Windows, strips the leading '/' and replaces '/' with '\'. */

EXPORT StringBuf uri_to_filename (const char * uri, bool use_locale)
{
    if (strncmp (uri, URI_PREFIX, URI_PREFIX_LEN))
        return StringBuf ();

    StringBuf buf = str_decode_percent (uri + URI_PREFIX_LEN);

#ifndef _WIN32
    /* convert to locale if:
     * 1) use_locale flag was not set to false, and
     * 2) system locale is not UTF-8, and
     * 3) decoded URI is valid UTF-8 */
    if (use_locale && ! g_get_charset (nullptr) && g_utf8_validate (buf, buf.len (), nullptr))
    {
        StringBuf locale = str_to_locale (buf);
        if (locale)
            buf.steal (std::move (locale));
    }
#endif

    return filename_normalize (std::move (buf));
}

/* Formats a URI for human-readable display.  Percent-decodes and, for file://
 * URI's, converts to filename format, but in UTF-8. */

EXPORT StringBuf uri_to_display (const char * uri)
{
    if (! strncmp (uri, "cdda://?", 8))
        return str_printf (_("Audio CD, track %s"), uri + 8);

    StringBuf buf = str_to_utf8 (str_decode_percent (uri));
    if (! buf)
        return str_copy (_("(character encoding error)"));

    if (strncmp (buf, URI_PREFIX, URI_PREFIX_LEN))
        return buf;

    buf.remove (0, URI_PREFIX_LEN);
    buf.steal (filename_normalize (std::move (buf)));

    const char * home = get_home_utf8 ();
    int homelen = home ? strlen (home) : 0;

    if (homelen && ! strncmp (buf, home, homelen) && buf[homelen] == G_DIR_SEPARATOR)
    {
        buf[0] = '~';
        buf.remove (1, homelen - 1);
    }

    return buf;
}

#undef URI_PREFIX
#undef URI_PREFIX_LEN

EXPORT void uri_parse (const char * uri, const char * * base_p, const char * * ext_p,
 const char * * sub_p, int * isub_p)
{
    const char * end = uri + strlen (uri);
    const char * base, * ext, * sub, * c;
    int isub = 0;
    char junk;

    if ((c = strrchr (uri, '/')))
        base = c + 1;
    else
        base = end;

    if ((c = strrchr (base, '?')) && sscanf (c + 1, "%d%c", & isub, & junk) == 1)
        sub = c;
    else
        sub = end;

    if ((c = strrchr (base, '.')) && c < sub)
        ext = c;
    else
        ext = sub;

    if (base_p)
        * base_p = base;
    if (ext_p)
        * ext_p = ext;
    if (sub_p)
        * sub_p = sub;
    if (isub_p)
        * isub_p = isub;
}

EXPORT StringBuf uri_get_scheme (const char * uri)
{
    const char * delim = strstr (uri, "://");
    return delim ? str_copy (uri, delim - uri) : StringBuf ();
}

EXPORT StringBuf uri_get_extension (const char * uri)
{
    const char * ext;
    uri_parse (uri, nullptr, & ext, nullptr, nullptr);

    if (ext[0] != '.')
        return StringBuf ();

    ext ++;  // skip period

    // remove subtunes and HTTP query strings
    const char * qmark = strchr (ext, '?');
    return str_copy (ext, qmark ? qmark - ext : -1);
}

/* Constructs a full URI given:
 *   1. path: one of the following:
 *     a. a full URI (returned unchanged)
 *     b. an absolute filename (in UTF-8 or the system locale)
 *     c. a relative path (character set detected according to user settings)
 *   2. reference: the full URI of the playlist containing <path> */

EXPORT StringBuf uri_construct (const char * path, const char * reference)
{
    /* URI */
    if (strstr (path, "://"))
        return str_copy (path);

    /* absolute filename */
#ifdef _WIN32
    if (path[0] && path[1] == ':' && (path[2] == '/' || path[2] == '\\'))
#else
    if (path[0] == '/')
#endif
        return filename_to_uri (path);

    /* relative path */
    const char * slash = strrchr (reference, '/');
    if (! slash)
        return StringBuf ();

    StringBuf buf = str_to_utf8 (path, -1);
    if (! buf)
        return buf;

    if (aud_get_bool (nullptr, "convert_backslash"))
        str_replace_char (buf, '\\', '/');

    buf.steal (str_encode_percent (buf));
    buf.insert (0, reference, slash + 1 - reference);
    return buf;
}

/* Like strcasecmp, but orders numbers correctly (2 before 10). */
/* Non-ASCII characters are treated exactly as is. */
/* Handles nullptr gracefully. */

EXPORT int str_compare (const char * ap, const char * bp)
{
    if (! ap)
        return bp ? -1 : 0;
    if (! bp)
        return 1;

    unsigned char a = * ap ++, b = * bp ++;
    for (; a || b; a = * ap ++, b = * bp ++)
    {
        if (a > '9' || b > '9' || a < '0' || b < '0')
        {
            if (a <= 'Z' && a >= 'A')
                a += 'a' - 'A';
            if (b <= 'Z' && b >= 'A')
                b += 'a' - 'A';

            if (a > b)
                return 1;
            if (a < b)
                return -1;
        }
        else
        {
            int x = a - '0';
            for (; (a = * ap) <= '9' && a >= '0'; ap ++)
                x = 10 * x + (a - '0');

            int y = b - '0';
            for (; (b = * bp) >= '0' && b <= '9'; bp ++)
                y = 10 * y + (b - '0');

            if (x > y)
                return 1;
            if (x < y)
                return -1;
        }
    }

    return 0;
}

/* Decodes percent-encoded strings, then compares then with str_compare. */

EXPORT int str_compare_encoded (const char * ap, const char * bp)
{
    if (! ap)
        return bp ? -1 : 0;
    if (! bp)
        return 1;

    unsigned char a = * ap ++, b = * bp ++;
    for (; a || b; a = * ap ++, b = * bp ++)
    {
        if (a == '%' && ap[0] && ap[1])
        {
            a = (FROM_HEX (ap[0]) << 4) | FROM_HEX (ap[1]);
            ap += 2;
        }
        if (b == '%' && bp[0] && bp[1])
        {
            b = (FROM_HEX (bp[0]) << 4) | FROM_HEX (bp[1]);
            bp += 2;
        }

        if (a > '9' || b > '9' || a < '0' || b < '0')
        {
            if (a <= 'Z' && a >= 'A')
                a += 'a' - 'A';
            if (b <= 'Z' && b >= 'A')
                b += 'a' - 'A';

            if (a > b)
                return 1;
            if (a < b)
                return -1;
        }
        else
        {
            int x = a - '0';
            for (; (a = * ap) <= '9' && a >= '0'; ap ++)
                x = 10 * x + (a - '0');

            int y = b - '0';
            for (; (b = * bp) >= '0' && b <= '9'; bp ++)
                y = 10 * y + (b - '0');

            if (x > y)
                return 1;
            if (x < y)
                return -1;
        }
    }

    return 0;
}

EXPORT Index<String> str_list_to_index (const char * list, const char * delims)
{
    char dmap[256] = {0};

    for (; * delims; delims ++)
        dmap[(unsigned char) (* delims)] = 1;

    Index<String> index;
    const char * word = nullptr;

    for (; * list; list ++)
    {
        if (dmap[(unsigned char) (* list)])
        {
            if (word)
            {
                index.append (String (str_copy (word, list - word)));
                word = nullptr;
            }
        }
        else
        {
            if (! word)
            {
                word = list;
            }
        }
    }

    if (word)
        index.append (String (word));

    return index;
}

EXPORT StringBuf index_to_str_list (const Index<String> & index, const char * sep)
{
    StringBuf str (-1);
    char * set = str;
    int left = str.len ();
    int seplen = strlen (sep);

    for (const String & s : index)
    {
        int len = strlen (s);
        if (len + seplen > left)
            throw std::bad_alloc ();

        if (set > str)
        {
            memcpy (set, sep, seplen);

            set += seplen;
            left -= seplen;
        }

        memcpy (set, s, len);

        set += len;
        left -= len;
    }

    str.resize (set - str);
    return str;
}

/*
 * Routines to convert numbers between string and binary representations.
 *
 * Goals:
 *
 *  - Accuracy, meaning that we can convert back and forth between string and
 *    binary without the number changing slightly each time.
 *  - Consistency, meaning that we get the same results no matter what
 *    architecture or locale we have to deal with.
 *  - Readability, meaning that the number one is rendered "1", not "1.000".
 *
 * Values between -1,000,000,000 and 1,000,000,000 (inclusive) are guaranteed to
 * have an accuracy of 6 decimal places.
 */

EXPORT int str_to_int (const char * string)
{
    bool neg = (string[0] == '-');
    if (neg)
        string ++;

    int val = 0;
    char c;

    while ((c = * string ++) && c >= '0' && c <= '9')
        val = val * 10 + (c - '0');

    return neg ? -val : val;
}

EXPORT double str_to_double (const char * string)
{
    bool neg = (string[0] == '-');
    if (neg)
        string ++;

    double val = str_to_int (string);
    const char * p = strchr (string, '.');

    if (p)
    {
        char buf[7] = "000000";
        memcpy (buf, p + 1, strlen_bounded (p + 1, 6));
        val += (double) str_to_int (buf) / 1000000;
    }

    return neg ? -val : val;
}

EXPORT StringBuf int_to_str (int val)
{
    bool neg = (val < 0);
    if (neg)
        val = -val;

    char buf[16];
    char * rev = buf + sizeof buf;

    while (rev > buf)
    {
        * (-- rev) = '0' + val % 10;
        if (! (val /= 10))
            break;
    }

    if (neg && rev > buf)
        * (-- rev) = '-';

    int len = buf + sizeof buf - rev;
    StringBuf buf2 (len);
    memcpy (buf2, rev, len);
    return buf2;
}

EXPORT StringBuf double_to_str (double val)
{
    bool neg = (val < 0);
    if (neg)
        val = -val;

    int i = floor (val);
    int f = round ((val - i) * 1000000);

    if (f == 1000000)
    {
        i ++;
        f = 0;
    }

    StringBuf buf = str_printf ("%s%d.%06d", neg ? "-" : "", i, f);

    char * c = buf + buf.len ();
    while (c[-1] == '0')
        c --;
    if (c[-1] == '.')
        c --;

    buf.resize (c - buf);
    return buf;
}

EXPORT bool str_to_int_array (const char * string, int * array, int count)
{
    Index<String> index = str_list_to_index (string, ", ");

    if (index.len () != count)
        return false;

    for (int i = 0; i < count; i ++)
        array[i] = str_to_int (index[i]);

    return true;
}

EXPORT StringBuf int_array_to_str (const int * array, int count)
{
    Index<String> index;

    for (int i = 0; i < count; i ++)
        index.append (String (int_to_str (array[i])));

    return index_to_str_list (index, ",");
}

EXPORT bool str_to_double_array (const char * string, double * array, int count)
{
    Index<String> index = str_list_to_index (string, ", ");

    if (index.len () != count)
        return false;

    for (int i = 0; i < count; i ++)
        array[i] = str_to_double (index[i]);

    return true;
}

EXPORT StringBuf double_array_to_str (const double * array, int count)
{
    Index<String> index;

    for (int i = 0; i < count; i ++)
        index.append (String (double_to_str (array[i])));

    return index_to_str_list (index, ",");
}

EXPORT StringBuf str_format_time (int64_t milliseconds)
{
    int hours = milliseconds / 3600000;
    int minutes = (milliseconds / 60000) % 60;
    int seconds = (milliseconds / 1000) % 60;

    if (hours)
        return str_printf ("%d:%02d:%02d", hours, minutes, seconds);
    else
    {
        bool zero = aud_get_bool (nullptr, "leading_zero");
        return str_printf (zero ? "%02d:%02d" : "%d:%02d", minutes, seconds);
    }
}
