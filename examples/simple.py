# simple.py - very simple example of plain DBAPI-2.0 usage
#
# currently used as test-me-stress-me script for psycopg 2.0
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

class SimpleQuoter(object):
    def sqlquote(x=None):
        return "'bar'"

import sys
import psycopg2

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dsn:", DSN
conn = psycopg2.connect(DSN)
print "Encoding for this connection is", conn.encoding

curs = conn.cursor()
curs.execute("SELECT 1 AS foo")
print curs.fetchone()
curs.execute("SELECT 1 AS foo")
print curs.fetchmany()
curs.execute("SELECT 1 AS foo")
print curs.fetchall()

conn.rollback()

sys.exit(0)

curs.execute("SELECT 1 AS foo", async=1)

curs.execute("SELECT %(foo)s AS foo", {'foo':'bar'})
curs.execute("SELECT %(foo)s AS foo", {'foo':None})
curs.execute("SELECT %(foo)f AS foo", {'foo':42})
curs.execute("SELECT %(foo)s AS foo", {'foo':SimpleQuoter()})
