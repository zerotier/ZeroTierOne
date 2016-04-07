# doc/module.mk
# Part of ZeroTier One, a software-defined network layer.
#
# Copyright © 2016 Ben Finney <ben+zerotier@benfinney.id.au>
# This is free software: you may copy, modify, and/or distribute this
# work under the terms of the GNU General Public License, version 3 or
# later as published by the Free Software Foundation.
# No warranty expressed or implied.
# See the file ‘LICENSE.txt’ for details.

# Makefile module for ZeroTier One documentation.

TEMPFILE_SUFFIX = .tmp
.INTERMEDIATE: ${DOCUMENTATION_DIR}/*${TEMPFILE_SUFFIX}

RST_SUFFIX = .txt
manpage_sections = 1 2 3 4 5 6 7 8
manpage_names += zerotier-one.8
manpage_names += zerotier-idtool.1
manpage_names += zerotier-cli.1
manpage_source_paths = $(addprefix ${DOC_DIR}/, \
	$(addsuffix ${RST_SUFFIX},${manpage_names}))
manpage_paths = $(addprefix ${DOC_DIR}/,${manpage_names})
manpage_encoding_stub = ${DOC_DIR}/manpage_encoding_declaration.UTF-8

GENERATED_FILES += $(addprefix ${DOC_DIR}/,\
	$(foreach section,${manpage_sections},*.${section}))

RST2MAN = rst2man
RST2MAN_OPTS =


.PHONY: doc
doc: manpages

.PHONY: manpages
manpages: ${manpage_paths}

%.1: %.1${RST_SUFFIX}
	$(RST2MAN) "$<" > "$@"${TEMPFILE_SUFFIX}
	cat ${manpage_encoding_stub} "$@"${TEMPFILE_SUFFIX} > "$@"

%.8: %.8${RST_SUFFIX}
	$(RST2MAN) "$<" > "$@"${TEMPFILE_SUFFIX}
	cat ${manpage_encoding_stub} "$@"${TEMPFILE_SUFFIX} > "$@"


# Local variables:
# coding: utf-8
# mode: makefile
# End:
# vim: fileencoding=utf-8 filetype=make :
