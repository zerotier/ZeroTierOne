#! /bin/sh
# Run this to generate the configure script etc.

set -eu

PQXXVERSION=$(./tools/extract_version)
PQXX_ABI=$(./tools/extract_version --abi)
PQXX_MAJOR=$(./tools/extract_version --major)
PQXX_MINOR=$(./tools/extract_version --minor)
echo "libpqxx version $PQXXVERSION"
echo "libpqxx ABI version $PQXX_ABI"

substitute() {
	sed -e "s/@PQXXVERSION@/$PQXXVERSION/g" \
		-e "s/@PQXX_MAJOR@/$PQXX_MAJOR/g" \
		-e "s/@PQXX_MINOR@/$PQXX_MINOR/g" \
		-e "s/@PQXX_ABI@/$PQXX_ABI/g" \
		"$1"
}


# Use templating system to generate various Makefiles.
expand_templates() {
	for template in "$@"
	do
		./tools/template2mak.py "$template" "${template%.template}"
	done
}


# We have two kinds of templates.  One uses our custom templating tool.  And
# a few others simply have some substitutions done.
expand_templates $(find -name \*.template)
substitute include/pqxx/version.hxx.template >include/pqxx/version.hxx
substitute include/pqxx/doc/mainpage.md.template >include/pqxx/doc/mainpage.md


autoheader
libtoolize --force --automake --copy
aclocal -I . -I config/m4
automake --add-missing --copy
autoconf

echo "Done."
