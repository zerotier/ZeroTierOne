SUBDIRS = include src test tools config doc
EXTRA_DIST = autogen.sh configitems README.md VERSION requirements.json

MAINTAINERCLEANFILES = \
    Makefile.in aclocal.m4 config.h.in config.log configure stamp-h.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libpqxx.pc

TESTS = tools/lint


# Generate ChangeLog from git history.  It goes all the way back through
# the project's git, bzr, svn, and cvs days.
dist-hook: ChangeLog

ChangeLog: configure.ac
	git log --stat --name-only --date=short --abbrev-commit >$@


# We use README.md, but automake expects plain README.
README: README.md
	ln -s $< $@
