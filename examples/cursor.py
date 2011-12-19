# cursor.py - how to subclass the cursor type
#
# Copyright (C) 2004-2010 Federico Di Gregorio  <fog@debian.org>
#
# psycopg2 is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# psycopg2 is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.

## put in DSN your DSN string

DSN = 'dbname=test'

## don't modify anything below this line (except for experimenting)

import sys
import psycopg2
import psycopg2.extensions

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dsn:", DSN
conn = psycopg2.connect(DSN)
print "Encoding for this connection is", conn.encoding


class NoDataError(psycopg2.ProgrammingError):
    """Exception that will be raised by our cursor."""
    pass

class Cursor(psycopg2.extensions.cursor):
    """A custom cursor."""

    def fetchone(self):
        """Like fetchone but raise an exception if no data is available.

        Note that to have .fetchmany() and .fetchall() to raise the same
        exception we'll have to override them too; even if internally psycopg
        uses the same function to fetch rows, the code path from Python is
        different.
        """
        d = psycopg2.extensions.cursor.fetchone(self)
        if d is None:
	    raise NoDataError("no more data")
        return d
    
curs = conn.cursor(cursor_factory=Cursor)
curs.execute("SELECT 1 AS foo")
print "Result of fetchone():", curs.fetchone()

# now let's raise the exception
try:
    curs.fetchone()
except NoDataError, err:
    print "Exception caught:", err  

conn.rollback()
