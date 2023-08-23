/* Compiler deficiency workarounds for compiling libpqxx headers.
 *
 * To be included at the end of each libpqxx header, in order to restore the
 * client program's settings.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
// NO GUARDS HERE! This code should be executed every time!

#if defined(_MSC_VER)
#  pragma warning(pop) // Restore compiler's warning state
#endif

#if !defined(PQXX_HEADER_PRE)
#  error "Include pqxx/internal/header-post.hxx AFTER its 'pre' counterpart."
#endif

#undef PQXX_HEADER_PRE
