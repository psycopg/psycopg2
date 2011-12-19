# fetch.py -- example about declaring cursors
#
# Copyright (C) 2001-2010 Federico Di Gregorio  <fog@debian.org>
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

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dns:", DSN
conn = psycopg2.connect(DSN)
print "Encoding for this connection is", conn.encoding

curs = conn.cursor()
try:
    curs.execute("CREATE TABLE test_fetch (val int4)")
except:
    conn.rollback()
    curs.execute("DROP TABLE test_fetch")
    curs.execute("CREATE TABLE test_fetch (val int4)")
conn.commit()

# we use this function to format the output

def flatten(l):
    """Flattens list of tuples l."""
    return map(lambda x: x[0], l)

# insert 20 rows in the table

for i in range(20):
    curs.execute("INSERT INTO test_fetch VALUES(%s)", (i,))
conn.commit()

# does some nice tricks with the transaction and postgres cursors
# (remember to always commit or rollback before a DECLARE)
#
# we don't need to DECLARE ourselves, psycopg now supports named
# cursors (but we leave the code here, comments, as an example of
# what psycopg is doing under the hood)
#
#curs.execute("DECLARE crs CURSOR FOR SELECT * FROM test_fetch")
#curs.execute("FETCH 10 FROM crs")
#print "First 10 rows:", flatten(curs.fetchall())
#curs.execute("MOVE -5 FROM crs")
#print "Moved back cursor by 5 rows (to row 5.)"
#curs.execute("FETCH 10 FROM crs")
#print "Another 10 rows:", flatten(curs.fetchall())
#curs.execute("FETCH 10 FROM crs")
#print "The remaining rows:", flatten(curs.fetchall())

ncurs = conn.cursor("crs")
ncurs.execute("SELECT * FROM test_fetch")
print "First 10 rows:", flatten(ncurs.fetchmany(10))
ncurs.scroll(-5)
print "Moved back cursor by 5 rows (to row 5.)"
print "Another 10 rows:", flatten(ncurs.fetchmany(10))
print "Another one:", list(ncurs.fetchone())
print "The remaining rows:", flatten(ncurs.fetchall())
conn.rollback()

curs.execute("DROP TABLE test_fetch")
conn.commit()
