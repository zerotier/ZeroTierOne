/* Compiler settings for compiling libpqxx itself.
 *
 * Include this header in every source file that goes into the libpqxx library
 * binary, and nowhere else.
 *
 * To ensure this, include this file once, as the very first header, in each
 * compilation unit for the library.
 *
 * DO NOT INCLUDE THIS FILE when building client programs.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */

// Workarounds & definitions needed to compile libpqxx into a library.
#include "pqxx/config-internal-compiler.h"

#ifdef _WIN32

#  ifdef PQXX_SHARED
// We're building libpqxx as a shared library.
#    undef PQXX_LIBEXPORT
#    define PQXX_LIBEXPORT __declspec(dllexport)
#    define PQXX_PRIVATE __declspec()
#  endif // PQXX_SHARED

#endif // _WIN32
