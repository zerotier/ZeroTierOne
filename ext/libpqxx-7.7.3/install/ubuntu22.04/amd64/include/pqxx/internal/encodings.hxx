/** Internal string encodings support for libpqxx
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_ENCODINGS
#define PQXX_H_ENCODINGS

#include "pqxx/internal/encoding_group.hxx"

#include <string>
#include <string_view>


namespace pqxx::internal
{
char const *name_encoding(int encoding_id);

/// Convert libpq encoding enum or encoding name to its libpqxx group.
encoding_group enc_group(int /* libpq encoding ID */);
encoding_group enc_group(std::string_view);


/// Look up the glyph scanner function for a given encoding group.
/** To identify the glyph boundaries in a buffer, call this to obtain the
 * scanner function appropriate for the buffer's encoding.  Then, repeatedly
 * call the scanner function to find the glyphs.
 */
PQXX_LIBEXPORT glyph_scanner_func *get_glyph_scanner(encoding_group);


// TODO: For ASCII search, treat UTF8/EUC_*/MULE_INTERNAL as MONOBYTE.

/// Find any of the ASCII characters `NEEDLE` in `haystack`.
/** Scans through `haystack` until it finds a single-byte character that
 * matches any value in `NEEDLE`.
 *
 * If it finds one, returns its offset.  If not, returns the end of the
 * haystack.
 */
template<char... NEEDLE>
inline std::size_t find_char(
  glyph_scanner_func *scanner, std::string_view haystack,
  std::size_t here = 0u)
{
  auto const sz{std::size(haystack)};
  auto const data{std::data(haystack)};
  while (here < sz)
  {
    auto next{scanner(data, sz, here)};
    // (For some reason gcc had a problem with a right-fold here.  But clang
    // was fine.)
    if ((... or (data[here] == NEEDLE)))
    {
      // Also check against a multibyte character starting with a bytes which
      // just happens to match one of the ASCII bytes we're looking for.  It'd
      // be cleaner to check that first, but either works.  So, let's apply the
      // most selective filter first and skip this check in almost all cases.
      if (next == here + 1)
        return here;
    }

    // Nope, no hit.  Move on.
    here = next;
  }
  return sz;
}


/// Iterate over the glyphs in a buffer.
/** Scans the glyphs in the buffer, and for each, passes its begin and its
 * one-past-end pointers to `callback`.
 */
template<typename CALLABLE>
inline void for_glyphs(
  encoding_group enc, CALLABLE callback, char const buffer[],
  std::size_t buffer_len, std::size_t start = 0)
{
  auto const scan{get_glyph_scanner(enc)};
  for (std::size_t here = start, next; here < buffer_len; here = next)
  {
    next = scan(buffer, buffer_len, here);
    callback(buffer + here, buffer + next);
  }
}
} // namespace pqxx::internal
#endif
