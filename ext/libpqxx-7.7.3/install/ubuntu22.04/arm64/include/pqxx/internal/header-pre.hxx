/* Compiler settings for compiling libpqxx headers, and workarounds for all.
 *
 * Include this before including any other libpqxx headers from within libpqxx.
 * And to balance it out, also include header-post.hxx at the end of the batch
 * of headers.
 *
 * The public libpqxx headers (e.g. `<pqxx/connection>`) include this already;
 * there's no need to do this from within an application.
 *
 * Include this file at the highest aggregation level possible to avoid nesting
 * and to keep things simple.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */

// NO GUARD HERE! This part should be included every time this file is.
#if defined(_MSC_VER)

// Save compiler's warning state, and set warning level 4 for maximum
// sensitivity to warnings.
#  pragma warning(push, 4)

// Visual C++ generates some entirely unreasonable warnings.  Disable them.
// Copy constructor could not be generated.
#  pragma warning(disable : 4511)
// Assignment operator could not be generated.
#  pragma warning(disable : 4512)
// Can't expose outside classes without exporting them.  Except the MSVC docs
// say please ignore the warning if it's a standard library class.
#  pragma warning(disable : 4251)
// Can't derive library classes from outside classes without exporting them.
// Except the MSVC docs say please ignore the warning if the parent class is
// in the standard library.
#  pragma warning(disable : 4275)
// Can't inherit from non-exported class.
#  pragma warning(disable : 4275)

#endif // _MSC_VER


#if defined(PQXX_HEADER_PRE)
#  error "Avoid nesting #include of pqxx/internal/header-pre.hxx."
#endif

#define PQXX_HEADER_PRE


// Workarounds & definitions that need to be included even in library's headers
#include "pqxx/config-public-compiler.h"

// Enable ISO-646 alternative operaotr representations: "and" instead of "&&"
// etc. on older compilers.  C++20 removes this header.
#if __has_include(<ciso646>)
#  include <ciso646>
#endif


#if defined(PQXX_HAVE_GCC_PURE)
/// Declare function "pure": no side effects, only reads globals and its args.
#  define PQXX_PURE __attribute__((pure))
#else
#  define PQXX_PURE /* pure */
#endif


#if defined(__GNUC__)
/// Tell the compiler to optimise a function for size, not speed.
#  define PQXX_COLD __attribute__((cold))
#else
#  define PQXX_COLD /* cold */
#endif


// Workarounds for Windows
#ifdef _WIN32

/* For now, export DLL symbols if _DLL is defined.  This is done automatically
 * by the compiler when linking to the dynamic version of the runtime library,
 * according to "gzh"
 */
#  if defined(PQXX_SHARED) && !defined(PQXX_LIBEXPORT)
#    define PQXX_LIBEXPORT __declspec(dllimport)
#  endif // PQXX_SHARED && !PQXX_LIBEXPORT


// Workarounds for Microsoft Visual C++
#  ifdef _MSC_VER

// Suppress vtables on abstract classes.
#    define PQXX_NOVTABLE __declspec(novtable)

// Automatically link with the appropriate libpq (static or dynamic, debug or
// release).  The default is to use the release DLL.  Define PQXX_PQ_STATIC to
// link to a static version of libpq, and _DEBUG to link to a debug version.
// The two may be combined.
#    if defined(PQXX_AUTOLINK)
#      if defined(PQXX_PQ_STATIC)
#        ifdef _DEBUG
#          pragma comment(lib, "libpqd")
#        else
#          pragma comment(lib, "libpq")
#        endif
#      else
#        ifdef _DEBUG
#          pragma comment(lib, "libpqddll")
#        else
#          pragma comment(lib, "libpqdll")
#        endif
#      endif
#    endif

// If we're not compiling libpqxx itself, automatically link with the
// appropriate libpqxx library.  To link with the libpqxx DLL, define
// PQXX_SHARED; the default is to link with the static library.  A static link
// is the recommended practice.
//
// The preprocessor macro PQXX_INTERNAL is used to detect whether we
// are compiling the libpqxx library itself.  When you compile the library
// yourself using your own project file, make sure to include this macro.
#    if defined(PQXX_AUTOLINK) && !defined(PQXX_INTERNAL)
#      ifdef PQXX_SHARED
#        ifdef _DEBUG
#          pragma comment(lib, "libpqxxD")
#        else
#          pragma comment(lib, "libpqxx")
#        endif
#      else // !PQXX_SHARED
#        ifdef _DEBUG
#          pragma comment(lib, "libpqxx_staticD")
#        else
#          pragma comment(lib, "libpqxx_static")
#        endif
#      endif
#    endif

#  endif // _MSC_VER

#elif defined(PQXX_HAVE_GCC_VISIBILITY) // !_WIN32

#  define PQXX_LIBEXPORT __attribute__((visibility("default")))
#  define PQXX_PRIVATE __attribute__((visibility("hidden")))

#endif // PQXX_HAVE_GCC_VISIBILITY


#ifndef PQXX_LIBEXPORT
#  define PQXX_LIBEXPORT /* libexport */
#endif

#ifndef PQXX_PRIVATE
#  define PQXX_PRIVATE /* private */
#endif

#ifndef PQXX_NOVTABLE
#  define PQXX_NOVTABLE /* novtable */
#endif

// C++20: Assume support.
#if defined(PQXX_HAVE_LIKELY)
#  define PQXX_LIKELY [[likely]]
#  define PQXX_UNLIKELY [[unlikely]]
#else
#  define PQXX_LIKELY   /* [[likely]] */
#  define PQXX_UNLIKELY /* [[unlikely]] */
#endif
