#!/bin/bash

# Run this file to package the .sql file into a .c file whenever the SQL changes.

rm -f schema.sql.c
echo '#define ZT_NETCONF_SCHEMA_SQL \' >schema.sql.c
cat schema.sql | sed 's/"/\\"/g' | sed 's/^/"/' | sed 's/$/\\n"\\/' >>schema.sql.c
echo '""' >>schema.sql.c
