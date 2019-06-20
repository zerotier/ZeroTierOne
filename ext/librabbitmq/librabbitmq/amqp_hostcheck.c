/*
 * Copyright 1996-2014 Daniel Stenberg <daniel@haxx.se>.
 * Copyright 2014 Michael Steinert
 *
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization of the
 * copyright holder.
 */

#include "amqp_hostcheck.h"

#include <string.h>

/* Portable, consistent toupper (remember EBCDIC). Do not use toupper()
 * because its behavior is altered by the current locale.
 */

static char amqp_raw_toupper(char in) {
  switch (in) {
    case 'a':
      return 'A';
    case 'b':
      return 'B';
    case 'c':
      return 'C';
    case 'd':
      return 'D';
    case 'e':
      return 'E';
    case 'f':
      return 'F';
    case 'g':
      return 'G';
    case 'h':
      return 'H';
    case 'i':
      return 'I';
    case 'j':
      return 'J';
    case 'k':
      return 'K';
    case 'l':
      return 'L';
    case 'm':
      return 'M';
    case 'n':
      return 'N';
    case 'o':
      return 'O';
    case 'p':
      return 'P';
    case 'q':
      return 'Q';
    case 'r':
      return 'R';
    case 's':
      return 'S';
    case 't':
      return 'T';
    case 'u':
      return 'U';
    case 'v':
      return 'V';
    case 'w':
      return 'W';
    case 'x':
      return 'X';
    case 'y':
      return 'Y';
    case 'z':
      return 'Z';
  }
  return in;
}

/*
 * amqp_raw_equal() is for doing "raw" case insensitive strings. This is meant
 * to be locale independent and only compare strings we know are safe for
 * this. See http://daniel.haxx.se/blog/2008/10/15/strcasecmp-in-turkish/ for
 * some further explanation to why this function is necessary.
 *
 * The function is capable of comparing a-z case insensitively even for
 * non-ascii.
 */

static int amqp_raw_equal(const char *first, const char *second) {
  while (*first && *second) {
    if (amqp_raw_toupper(*first) != amqp_raw_toupper(*second)) {
      /* get out of the loop as soon as they don't match */
      break;
    }
    first++;
    second++;
  }
  /* we do the comparison here (possibly again), just to make sure that if
   * the loop above is skipped because one of the strings reached zero, we
   * must not return this as a successful match
   */
  return (amqp_raw_toupper(*first) == amqp_raw_toupper(*second));
}

static int amqp_raw_nequal(const char *first, const char *second, size_t max) {
  while (*first && *second && max) {
    if (amqp_raw_toupper(*first) != amqp_raw_toupper(*second)) {
      break;
    }
    max--;
    first++;
    second++;
  }
  if (0 == max) {
    return 1; /* they are equal this far */
  }
  return amqp_raw_toupper(*first) == amqp_raw_toupper(*second);
}

/*
 * Match a hostname against a wildcard pattern.
 * E.g.
 *  "foo.host.com" matches "*.host.com".
 *
 * We use the matching rule described in RFC6125, section 6.4.3.
 * http://tools.ietf.org/html/rfc6125#section-6.4.3
 */

static amqp_hostcheck_result amqp_hostmatch(const char *hostname,
                                            const char *pattern) {
  const char *pattern_label_end, *pattern_wildcard, *hostname_label_end;
  int wildcard_enabled;
  size_t prefixlen, suffixlen;
  pattern_wildcard = strchr(pattern, '*');
  if (pattern_wildcard == NULL) {
    return amqp_raw_equal(pattern, hostname) ? AMQP_HCR_MATCH
                                             : AMQP_HCR_NO_MATCH;
  }
  /* We require at least 2 dots in pattern to avoid too wide wildcard match. */
  wildcard_enabled = 1;
  pattern_label_end = strchr(pattern, '.');
  if (pattern_label_end == NULL || strchr(pattern_label_end + 1, '.') == NULL ||
      pattern_wildcard > pattern_label_end ||
      amqp_raw_nequal(pattern, "xn--", 4)) {
    wildcard_enabled = 0;
  }
  if (!wildcard_enabled) {
    return amqp_raw_equal(pattern, hostname) ? AMQP_HCR_MATCH
                                             : AMQP_HCR_NO_MATCH;
  }
  hostname_label_end = strchr(hostname, '.');
  if (hostname_label_end == NULL ||
      !amqp_raw_equal(pattern_label_end, hostname_label_end)) {
    return AMQP_HCR_NO_MATCH;
  }
  /* The wildcard must match at least one character, so the left-most
   * label of the hostname is at least as large as the left-most label
   * of the pattern.
   */
  if (hostname_label_end - hostname < pattern_label_end - pattern) {
    return AMQP_HCR_NO_MATCH;
  }
  prefixlen = pattern_wildcard - pattern;
  suffixlen = pattern_label_end - (pattern_wildcard + 1);
  return amqp_raw_nequal(pattern, hostname, prefixlen) &&
                 amqp_raw_nequal(pattern_wildcard + 1,
                                 hostname_label_end - suffixlen, suffixlen)
             ? AMQP_HCR_MATCH
             : AMQP_HCR_NO_MATCH;
}

amqp_hostcheck_result amqp_hostcheck(const char *match_pattern,
                                     const char *hostname) {
  /* sanity check */
  if (!match_pattern || !*match_pattern || !hostname || !*hostname) {
    return AMQP_HCR_NO_MATCH;
  }
  /* trivial case */
  if (amqp_raw_equal(hostname, match_pattern)) {
    return AMQP_HCR_MATCH;
  }
  return amqp_hostmatch(hostname, match_pattern);
}
