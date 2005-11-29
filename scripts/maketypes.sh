#!/bin/sh

SCRIPTSDIR="`dirname $0`"
SRCDIR="`dirname $SCRIPTSDIR`/psycopg"

if [ -z "$1" ] ; then
    echo Usage: $0 '<postgresql include directory>'
    exit 1
fi

echo -n checking for pg_type.h ...
if [ -f "$1/catalog/pg_type.h" ] ; then
    PGTYPE="$1/catalog/pg_type.h"
else
    if [ -f "$1/server/catalog/pg_type.h" ] ; then
        PGTYPE="$1/server/catalog/pg_type.h"
    else
	echo
        echo "error: can't find pg_type.h under $1"
	exit 2
    fi
fi
echo " found"

PGVERSION="`sed -n -e 's/.*PG_VERSION \"\([0-9]\.[0-9]\).*\"/\1/p' $1/pg_config.h`"
PGMAJOR="`echo $PGVERSION | cut -d. -f1`"
PGMINOR="`echo $PGVERSION | cut -d. -f2`"

echo checking for postgresql major: $PGMAJOR
echo checking for postgresql minor: $PGMINOR
    
echo -n generating pgtypes.h ...
awk '/#define .+OID/ {print "#define " $2 " " $3}' "$PGTYPE" \
    > $SRCDIR/pgtypes.h
echo " done"
echo -n generating typecast_builtins.c ...
awk '/#define .+OID/ {print $2 " " $3}' "$PGTYPE" | \
    python $SCRIPTSDIR/buildtypes.py >$SRCDIR/typecast_builtins.c
echo " done"


