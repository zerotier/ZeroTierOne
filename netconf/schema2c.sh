#!/bin/bash

# Run this file to package the .sql file into a .c file whenever the SQL changes.

rm -f netconf-schema.sql.c
echo '#define ZT_NETCONF_SCHEMA_SQL \' >netconf-schema.sql.c
cat netconf-schema.sql | sed 's/"/\\"/g' | sed 's/^/"/' | sed 's/$/\\n"\\/' >>netconf-schema.sql.c
echo '""' >>netconf-schema.sql.c
