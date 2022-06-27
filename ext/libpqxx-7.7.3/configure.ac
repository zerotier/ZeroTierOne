# Generate configure script for libpqxx.  This needs the autoconf archive
# package installed.  (The configure script itself does not require it though.)

AC_PREREQ(2.69)
AC_INIT(
	libpqxx,
	[m4_esyscmd_s([./tools/extract_version])],
	[Jeroen T. Vermeulen])
AC_LANG(C++)

AC_CONFIG_SRCDIR([src/connection.cxx])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR([config/m4])
AM_INIT_AUTOMAKE([subdir-objects])

PQXX_ABI=m4_esyscmd_s([./tools/extract_version --abi])
AC_SUBST(PQXXVERSION, $PACKAGE_VERSION)
AC_SUBST(PQXX_ABI)

AC_CONFIG_HEADER([include/pqxx/config.h])

# Default prefix for installs.
AC_PREFIX_DEFAULT(/usr/local)


# Read test programme from config-test.
AC_DEFUN([read_test], [AC_LANG_SOURCE(
	esyscmd(tools/m4esc.py --input=config-tests/$1))])


# Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_DISABLE_SHARED
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_PATH_PROG([MKDIR], [mkdir])

# Documentation.
AC_ARG_ENABLE(
	documentation,
	[AS_HELP_STRING([--enable-documentation], [Generate documentation])],
	[],
	[enable_documentation=auto])
AC_ARG_VAR([DOXYGEN],
	[Path to doxygen needed to build reference documentation])
AC_PATH_TOOL([DOXYGEN], [doxygen], [nodoxygen])
AC_ARG_VAR([HAVE_DOT],
	[Variable used by doxygen to declare availibility of dot])
AC_CHECK_TOOL([HAVE_DOT], [dot], [YES], [NO])
AS_IF([test "$enable_documentation" = "yes" && test "$DOXYGEN" = "nodoxygen"],
      [AC_MSG_ERROR([could not find tools necessary to build documentation])])
AM_CONDITIONAL([BUILD_REFERENCE],
	[test "$enable_documentation" != "no" -a "$DOXYGEN" != "nodoxygen"])

AM_MAINTAINER_MODE

# See if we want stricter compiler warnings.
AC_MSG_CHECKING([maintainer mode])
AC_ARG_ENABLE(maintainer-mode)
AC_MSG_RESULT(${enable_maintainer_mode})

# See if we want runtime debug checking.
AC_MSG_CHECKING([audit])
AC_ARG_ENABLE(audit)
AC_MSG_RESULT(${enable_audit})

# See if we want "suggestions," such as "this class could be final."
# (The suggestions are often useful, but can also easily be wrong.)
AC_MSG_CHECKING([suggest])
AC_ARG_ENABLE(suggest)
AC_MSG_RESULT(${enable_suggest})


AC_ARG_ENABLE(shared)
AS_IF(
    [test "${shared}" = "yes" ],
    [CPPFLAGS="$CPPFLAGS -DPQXX_SHARED"])


# Add options to compiler command line, if compiler accepts them.
add_compiler_opts_if_ok() {
	for option in $*
	do
		ACO_SAVE_CXXFLAGS="$CXXFLAGS"
		CXXFLAGS="$CXXFLAGS $option"
		AC_MSG_CHECKING([whether $CXX accepts $option])
		AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM([], [])],
			has_option=yes,
			has_option=no)
		AC_MSG_RESULT($has_option)
		AS_IF(
		    [test "$has_option" = "no" ],
		    [CXXFLAGS="$ACO_SAVE_CXXFLAGS"])
	done
}


# Add options to compiler command line, unconditionally.
add_compiler_opts() {
	CXXFLAGS="$CXXFLAGS $*"
}


# It's tempting to use Autoconf Archive's AX_CXX_COMPILE_STDCXX_17 for this,
# but it's 2022 and the C++20 equivalent isn't quite ready for use.
# Seems simpler and more reliable for the user to arrange for the desired
# language versions by setting the appropriate option for their compiler.
AC_MSG_CHECKING([for sufficient C++ language/library level])
sufficient_cxx=yes
AC_COMPILE_IFELSE(
	[AC_LANG_SOURCE([
	#if __cplusplus < 201611L
	#error "Need C++17 or better."
	#endif
	])],
	sufficient_cxx=yes,
	sufficient_cxx=no)
AC_MSG_RESULT($sufficient_cxx)
if test "$sufficient_cxx" != "yes"
then
	AC_MSG_ERROR([This libpqxx version needs at least C++17.])
fi


# Let's try to get the compiler to be helpful.
#
# (Omit options -Weffc++ and -Wabi because they currently yield too many
# warnings in gcc's standard headers; omit -Wunreachable-code because it isn't
# always right)
if test "$GCC" = "yes"
then
	# In maintainer mode, enable all the warning options we can.
	if test "$enable_maintainer_mode" = "yes"
	then
		# "Eternal" (FLW) g++ options.  These have been around for
		# ages, and both g++ and clang++ support them.  Don't bother
		# checking for support; just add them to the compiler options.
		add_compiler_opts \
			-fstrict-enums \
			-Werror \
			-Wall \
			-pedantic \
			-Wcast-align  \
			-Wcast-qual  \
			-Wconversion \
			-Wctor-dtor-privacy \
			-Wendif-labels \
			-Wextra \
			-Wfloat-equal \
			-Wformat=2 \
			-Wformat-security \
			-Wmissing-include-dirs \
			-Wno-div-by-zero \
			-Wnon-virtual-dtor \
			-Wold-style-cast \
			-Woverlength-strings \
			-Woverloaded-virtual \
			-Wpointer-arith  \
			-Wredundant-decls \
			-Wshadow \
			-Wsign-promo \
			-Wundef \
			-Wunused \
			-Wwrite-strings

		# "Iffy" g++ options.  Some reasonably current g++-like
		# compilers may not support these.
		add_compiler_opts_if_ok \
			-fnothrow-opt \
			-Wattribute-alias=2 \
			-Wextra-semi \
			-Wlogical-op \
			-Wmismatched-tags \
			-Wnoexcept \
			-Wredundant-tags \
			-Wrestrict \
			-Wstringop-overflow \
			-Wzero-as-null-pointer-constant \
			-Warray-bounds=2 \
			-Wduplicated-branches \
			-Wduplicated-cond \
			-Wsuggest-attribute=noreturn \
			-Wsuggest-override \
			-Wtrampolines
	fi

	# In "audit," enable all runtime checks we can.
	if test "$enable_audit" = "yes"
	then
		add_compiler_opts_if_ok \
			-D_FORTIFY_SOURCE=2 \
			-fsanitize=address \
			-fsanitize-address-use-after-scope \
			-fsanitize=alignment \
			-fsanitize=bool \
			-fsanitize=bounds \
			-fsanitize=bounds-strict \
			-fsanitize=builtin \
			-fsanitize=enum \
			-fsanitize=float-cast-overflow \
			-fsanitize=float-divide-by-zero \
			-fsanitize=integer-divide-by-zero \
			-fsanitize=leak \
			-fsanitize=nonnull-attribute \
			-fsanitize=null \
			-fsanitize=object-size \
			-fsanitize=pointer-compare \
			-fsanitize=pointer-overflow \
			-fsanitize=pointer-subtract \
			-fsanitize=return \
			-fsanitize=returns-nonnull-attribute \
			-fsanitize=shift \
			-fsanitize=shift-base \
			-fsanitize=shift-exponent \
			-fsanitize=signed-integer-overflow \
			-fsanitize=undefined \
			-fsanitize=unreachable \
			-fsanitize=vla-bound \
			-fsanitize=vptr \
			-fstack-protector-all
	fi

	# In "suggest" mode, enable a bunch of code suggestions.
	if test "$enable_suggest" = "yes"
	then
		add_compiler_opts_if_ok \
			-Wsuggest-attribute=cold \
			-Wsuggest-attribute=const \
			-Wsuggest-attribute=malloc \
			-Wsuggest-attribute=pure \
			-Wsuggest-final-types \
			-Wsuggest-final-methods
	fi

AC_MSG_CHECKING([g++ visibility attribute])
gcc_visibility=yes
SAVE_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS -Werror"
AC_COMPILE_IFELSE(
	[read_test(gcc_visibility.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_GCC_VISIBILITY],
		1,
		[Define if g++ supports visibility attribute.]),
	gcc_visibility=no)
AC_MSG_RESULT($gcc_visibility)
CXXFLAGS="$SAVE_CXXFLAGS"
if test "$gcc_visibility" = "yes"
then
    # Make internal definitions accessible only to the library itself.
    # Only definitions marked PQXX_LIBEXPORT will be accessible.
    add_compiler_opts -fvisibility=hidden
    add_compiler_opts -fvisibility-inlines-hidden
fi

AC_MSG_CHECKING([g++ pure attribute])
gcc_pure=yes
AC_COMPILE_IFELSE(
	[read_test(gcc_pure.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_GCC_PURE],
		1,
		[Define if g++ supports pure attribute]),
	gcc_pure=no)
AC_MSG_RESULT($gcc_pure)

fi # End of gcc-specific part.


# Check for __cxa_demangle.
AC_MSG_CHECKING([__cxa_demangle])
cxa_demangle=yes
AC_COMPILE_IFELSE(
	[read_test(cxa_demangle.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_CXA_DEMANGLE],
		1,
		[Define if compiler supports __cxa_demangle]),
	cxa_demangle=no)
AC_MSG_RESULT($cxa_demangle)


# Check for sufficient Concepts support, introduced with C++20.
AC_MSG_CHECKING([concepts])
concepts=yes
AC_COMPILE_IFELSE(
	[read_test(concepts.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_CONCEPTS],
		1,
		[Define if compiler supports Concepts and <ranges> header.]),
	concepts=no)
AC_MSG_RESULT($concepts)


# Check for C++20 std::span.
AC_MSG_CHECKING([std::span])
span=yes
AC_COMPILE_IFELSE(
	[read_test(span.cxx)],
	AC_DEFINE([PQXX_HAVE_SPAN], 1, [Define if compiler has std::span.]),
	span=no)
AC_MSG_RESULT($span)


# Check for multidimensional subscript operator support.  Proposed for C++23.
AC_MSG_CHECKING([for multidimensional subscript operator support])
multidim_subscript=yes
AC_COMPILE_IFELSE(
	[read_test(multidim-subscript.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_MULTIDIMENSIONAL_SUBSCRIPT], 1,
		[Define if operator[] can take multiple arguments.]),
	multidim_subscript=no)
AC_MSG_RESULT($multidim_subscript)


AC_MSG_CHECKING([for strerror_r()])
strerror_r=yes
AC_LINK_IFELSE(
	[read_test(strerror_r.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_STRERROR_R],
		1,
		[Define if strerror_r() is available.]),
	strerror_r=no)
AC_MSG_RESULT($strerror_r)



AC_MSG_CHECKING([for strerror_s()])
strerror_s=yes
AC_LINK_IFELSE(
	[read_test(strerror_s.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_STRERROR_S],
		1,
		[Define if strerror_s() is available.]),
	strerror_s=no)
AC_MSG_RESULT($strerror_s)


AC_MSG_CHECKING([for std::chrono::year_month_day etc])
year_month_day=yes
AC_LINK_IFELSE(
	[read_test(year_month_day.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_YEAR_MONTH_DAY],
		1,
		[Define if std::chrono has year_month_day etc.]),
	year_month_day=no)
AC_MSG_RESULT($year_month_day)


# Check for [[likely]] and [[unlikely]] attributes.
AC_MSG_CHECKING([attributes "likely" and "unlikely".])
likely=yes
AC_COMPILE_IFELSE(
	[read_test(likely.cxx)],
	AC_DEFINE([PQXX_HAVE_LIKELY], 1, [Define if likely & unlikely work.]),
	likely=no)
AC_MSG_RESULT($likely)


# It's mid-2019, and gcc's charconv supports integers but not yet floats.
# So for now, we test for int and float conversion... separately.
#
# It's worse for older clang versions, which compile the integer conversions
# but then fail at link time because of a missing symbol "__muloti4" with the
# "long long" version.  I couldn't resolve that symbol by adding -lm either.
# So don't just compile these tests; link them as well.
AC_MSG_CHECKING([for C++17 charconv integer conversion])
have_charconv_int=yes
AC_LINK_IFELSE(
	[read_test(charconv_int.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_CHARCONV_INT],
		1,
		[Define if <charconv> supports integer conversion.]),
	have_charconv_int=no)
AC_MSG_RESULT($have_charconv_int)

AC_MSG_CHECKING([for C++17 charconv floating-point conversion])
have_charconv_float=yes
AC_LINK_IFELSE(
	[read_test(charconv_float.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_CHARCONV_FLOAT],
		1,
		[Define if <charconv> supports floating-point conversion.]),
	have_charconv_float=no)
AC_MSG_RESULT($have_charconv_float)

# As per #262, clang with libcxxrt does not support thread_local on non-POD
# objects.  Luckily we can live without those, it's just less efficient.
AC_MSG_CHECKING([for full thread_local support])
have_thread_local=yes
AC_LINK_IFELSE(
	[read_test(thread_local.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_THREAD_LOCAL],
		1,
		[Define if thread_local is fully supported.]),
	have_thread_local=no)
AC_MSG_RESULT($have_thread_local)

AC_MSG_CHECKING([for std::this_thread::sleep_for])
have_sleep_for=yes
AC_LINK_IFELSE(
	[read_test(sleep_for.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_SLEEP_FOR],
		1,
		[Define if std::this_thread::sleep_for works.]),
	have_sleep_for=no)
AC_MSG_RESULT($have_sleep_for)


AC_MSG_CHECKING([for std::cmp_greater, std::cmp_less_equal, etc])
have_cmp=yes
AC_COMPILE_IFELSE(
	[read_test(cmp.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_CMP],
		1,
		[Define if compiler has C++20 std::cmp_greater etc.]),
	have_cmp=no)
AC_MSG_RESULT($have_cmp)


# Doing my own check for poll().  There's one built into autoconf-archive, but
# it produces warnings in C++ (about unnecessarily using "struct", and using 0
# as a null pointer constant).  In maintainer mode, those warnings turn into
# errors.
AC_MSG_CHECKING([for poll()])
ax_cv_have_poll=yes
AC_LINK_IFELSE(
	[read_test(poll.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_POLL],
		1,
		[Define if poll() is available.]),
	ax_cv_have_poll=no)
AC_MSG_RESULT($ax_cv_have_poll)

if test "$ax_cv_have_poll" != "yes"
then
# No poll(); we'll fall back to select().

# Some systems keep select() in a separate library which is not linked by
# default.  See if we need one of those.
socklibok=no
AC_SEARCH_LIBS(select, socket nsl ws2_32 wsock32 winsock, [socklibok=yes])

# Microsoft proprietary libraries do not work with code that is generated with
# autoconf's SEARCH_LIBS macro, so we need to check manually and just use the
# first socket library available.
# We only do this if select() is not available by other means, to avoid picking
# up an unnecessary Windows compatibility library on a non-Windows system.
for l in ws2_32 wsock32 winsock
do
	if test "${socklibok}" != "yes"
	then
		AC_CHECK_LIB($l,main,LIBS="$LIBS -l$l";[socklibok=yes])
	fi
done

if test "${socklibok}" != "yes"
then
	AC_MSG_ERROR([
Could not figure out how to link a simple sockets-based program.  Please read
the config.log file for more clues as to why this failed.
])
fi

fi # No poll()


# Find PostgreSQL includes and libraries
AC_PATH_PROG([PKG_CONFIG], [pkg-config])
AC_PATH_PROGS(PG_CONFIG, pg_config)

AC_ARG_WITH(
    [postgres-include],
    [AS_HELP_STRING(
        [--with-postgres-include=DIR],
        [Use PostgreSQL includes from DIR.  Defaults to querying pg_config or pkg-config, whichever is available.])],
    AS_IF(
        [test "x$with_postgres_include" = "xyes"],
        [with_postgres_include=""]))

if test -n "$with_postgres_include"
then
    POSTGRES_INCLUDE="-I$with_postgres_include"
else
    if test -x "$PKG_CONFIG" || test -x "$PG_CONFIG"
    then
        # We should prefer pkg-config over pg_config, but there seems to be a
        # problem in pkg-config 1.6.3.  Until that's been resolved (#291), go
        # with pg_config if we can.
        if test -x "$PG_CONFIG"
        then
            # From pg_config we can either get the C compiler options used to
            # compile postgres, which isn't quite what we want; or we can get
            # the headers directory, without the full option.  That's something
            # we can work with.  The compiler must support the "-I" option for
            # that, but both scripts assume that anyway.
            POSTGRES_INCLUDE="-I$($PG_CONFIG --includedir)"
        else
            # From pkg-config we can get the compiler options to extend the
            # include path.  We use that.
            POSTGRES_INCLUDE=$($PKG_CONFIG libpq --cflags-only-I)
        fi
        AC_MSG_NOTICE([finding PostgreSQL headers using $POSTGRES_INCLUDE])
    else
        POSTGRES_INCLUDE=""

        # We have nothing to tell us where the libpq headers are.  That's fine
        # if the compiler can find it, but if not, fail here.
        AC_CHECK_HEADER(
	        [libpq-fe.h],
	        [],
	[AC_MSG_ERROR([
Can't find the main PostgreSQL client header, libpq-fe.h.  Make sure that it
is installed, and either use the --with-postgres-include option or install
pkg-config.
])])
    fi
fi
AC_SUBST(POSTGRES_INCLUDE)

# Add the compiler option so we can compile configure tests which rely on the
# libpq headers.
CPPFLAGS="$CPPFLAGS $POSTGRES_INCLUDE"


AC_ARG_WITH(
    [postgres-lib],
    [AS_HELP_STRING(
        [--with-postgres-lib=DIR],
        [Use PostgreSQL libraries from DIR.  Defaults to querying pg_config.])],
    AS_IF(
        [test "x$with_postgres_lib" = "xyes"],
        [with_postgres_lib=""]))

# If no --with-postgres-lib was given, and we have pkg-config, use that.
AS_IF(
    [test -z "$with_postgres_lib" -a -x "$PKG_CONFIG"],
    [with_postgres_lib=$($PKG_CONFIG libpq --libs-only-L | sed 's/^-L//')])

# pg_config is deprecated, but for some users it may still provide the only
# right answer.  For instance, `pkg-config` may not know where `libpq` is
# installed.
AS_IF(
    [test -z "$with_postgres_lib" -a -x "$PG_CONFIG"],
    [with_postgres_lib=$($PG_CONFIG --libdir)])

AS_IF(
    [test -n "$with_postgres_lib"],
    [AC_MSG_NOTICE([using PostgreSQL libraries at $with_postgres_lib])],
    [AC_MSG_NOTICE([using PostgreSQL libraries in default location])])

AC_SUBST(with_postgres_lib)


AC_CHECK_HEADER(
	[libpq-fe.h],
	[],
	[AC_MSG_ERROR([
Can't find the main PostgreSQL client header, libpq-fe.h.  Are you sure the
libpq headers are installed correctly, and that we've got the right path?
])])


AC_MSG_CHECKING([for ability to compile source files using libpq])
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include<libpq-fe.h>]],
		[[PQexec(nullptr,"")]]
	)],
	[],
	[AC_MSG_ERROR([
Could not compile a call to a basic libpq function.  There must be something
seriously wrong with the headers that "pg_config --includedir" or "pkg-config
libpq --cflags" pointed to; the contents of config.log may give you a clue
about the nature of the failure.
Source including the libpq header libpq-fe.h can be compiled, but a call to the
most basic libpq function PQexec() failed to compile successfully.  This is the
litmus test for a working libpq.
])])
AC_MSG_RESULT(yes)


if test "x${with_postgres_lib}" = "x"; then
    with_postgres_libpath=""
else
    with_postgres_libpath="-L${with_postgres_lib}"
fi
LDFLAGS="$LDFLAGS ${with_postgres_libpath}"

AC_CHECK_LIB(
	[pq],
	[PQexec],
	[],
	[AC_MSG_ERROR([
Did not find the PQexec() function in libpq.  This is the litmus test for a
working libpq installation.

A source file using the PQexec() function did compile without problems, and the
libpq library is available for linking, but for some reason a call to PQexec()
failed to link properly to the libpq library.  This may be because the libpq
library file is damaged, or in some incorrect format, or if your libpq is much
more recent than libpqxx version $PQXX_ABI, perhaps libpq has undergone a
radical ABI change.

The last parts of config.log may give you a clue as to what really went wrong,
but be warned that this is no easy reading.  Look for the last error message
occurring in the file.
])],
	${with_postgres_libpath})


# PQencryptPasswordConn was added in postgres 10.
AC_MSG_CHECKING([for PQencryptPasswordConn])
have_pqencryptpasswordconn=yes
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[#include<libpq-fe.h>],
		[
			extern PGconn *conn;
			PQencryptPasswordConn(
				conn, "pwd", "user", "scram-sha-256")
		]
	)],
	AC_DEFINE(
		[PQXX_HAVE_PQENCRYPTPASSWORDCONN],
		1,
		[Define if libpq has PQencryptPasswordConn (since pg 10).]),
	[have_pqencryptpasswordconn=no])
AC_MSG_RESULT($have_pqencryptpasswordconn)


# "Pipeline mode" was introduced in libpq 14.
AC_MSG_CHECKING([for libpq pipeline mode])
have_pq_pipeline=yes
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[#include<libpq-fe.h>],
		[
			extern PGconn *conn;
			PQenterPipelineMode(conn)
		]
	)],
	AC_DEFINE(
		[PQXX_HAVE_PQ_PIPELINE],
		1,
		[Define if libpq has pipeline mode (since pg 14).]),
	[have_pq_pipeline=no])
AC_MSG_RESULT($have_pq_pipeline)


AC_MSG_CHECKING([for usable std::filesystem::path])
have_path=yes
AC_COMPILE_IFELSE(
	[read_test(fs.cxx)],
	AC_DEFINE(
		[PQXX_HAVE_PATH],
		1,
		[Define if compiler has usable std::filesystem::path.]),
	have_path=no)
AC_MSG_RESULT($have_path)


AC_MSG_CHECKING([whether we need a link option for <filesystem> support])
LIBS_SAVE="$LIBS"
found_fslib=no
for l in '' '-lstdc++fs' '-lc++fs'
do
	if test "$found_fslib" != "yes"
	then
		LIBS="$LIBS $l"
		AC_LINK_IFELSE(
			[read_test(need_fslib.cxx)],
			[found_fslib=yes],
			[LIBS="$LIBS_SAVE"])
		if test "$found_fslib" = "yes"
		then
			result_msg="$l"
			# (And keep our current value of $LIBS.)
		fi
	fi
done

if test "$found_fslib" != "yes"
then
	AC_MSG_ERROR([
There seems to be <filesystem> support, but I could not figure out now to make
it work.  You'll have to add set your own build options for this.
	])
fi
AC_MSG_RESULT($result_msg)


# Remove redundant occurrances of -lpq
LIBS=[$(echo "$LIBS" | sed -e 's/-lpq * -lpq\>/-lpq/g')]


AC_MSG_CHECKING([that type of libpq's Oid is as expected])
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[
			#include<libpq-fe.h>
			#include"${srcdir}/include/pqxx/internal/libpq-forward.hxx"
			extern void f(pqxx::oid&);
		],
		[Oid o;f(o)],
	)],
	[],
	[AC_MSG_ERROR([
The Oid typedef in libpq has changed.  Please notify the libpqxx authors of the
change!
])])
AC_MSG_RESULT(yes)


AC_PROG_MAKE_SET

AC_CONFIG_FILES([
	Makefile config/Makefile doc/Makefile doc/Doxyfile src/Makefile
	test/Makefile tools/Makefile include/Makefile include/pqxx/Makefile
	libpqxx.pc])


AC_CONFIG_COMMANDS([configitems], ["${srcdir}/tools/splitconfig" "${srcdir}"])

AC_OUTPUT(compile_flags)
