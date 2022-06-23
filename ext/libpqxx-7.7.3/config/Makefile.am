EXTRA_DIST=m4/Makefile.am sample-headers
MAINTAINERCLEANFILES=Makefile.in config.guess config.sub install-sh \
	ltmain.sh missing mkinstalldirs

dist-hook:
	find "${distdir}" -type d -name CVS -print0 | xargs -0 rm -rf
	find "${distdir}" -type d -name .svn -print0 | xargs -0 rm -rf

