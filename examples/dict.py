# dict.py - using DictCUrsor/DictRow
#
# Copyright (C) 2005-2010 Federico Di Gregorio  <fog@debian.org>
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
import psycopg2.extras

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dsn:", DSN
conn = psycopg2.connect(DSN)
print "Encoding for this connection is", conn.encoding

    
curs = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
curs.execute("SELECT 1 AS foo, 'cip' AS bar, date(now()) as zot")
print "Cursor's row factory is", curs.row_factory

data = curs.fetchone()
print "The type of the data row is", type(data)
print "Some data accessed both as tuple and dict:"
print " ", data['foo'], data['bar'], data['zot']
print " ", data[0], data[1], data[2]

# execute another query and demostrate we can still access the row
curs.execute("SELECT 2 AS foo")
print "The type of the data row is", type(data)
print "Some more data accessed both as tuple and dict:"
print " ", data['foo'], data['bar'], data['zot']
print " ", data[0], data[1], data[2]

curs = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
curs.execute("SELECT 1 AS foo, 'cip' AS bar, date(now()) as zot")
print "Cursor's row factory is", curs.row_factory

data = curs.fetchone()
print "The type of the data row is", type(data)
print "Some data accessed both as tuple and dict:"
print " ", data['foo'], data['bar'], data['zot']
print " ", "No access using indices: this is a specialized cursor."

# execute another query and demostrate we can still access the row
curs.execute("SELECT 2 AS foo")
print "The type of the data row is", type(data)
print "Some more data accessed both as tuple and dict:"
print " ", data['foo'], data['bar'], data['zot']
print " ", "No access using indices: this is a specialized cursor."
