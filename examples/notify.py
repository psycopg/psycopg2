# notify.py - example of getting notifies
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
import select
import psycopg2
from psycopg2.extensions import ISOLATION_LEVEL_AUTOCOMMIT

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dns:", DSN
conn = psycopg2.connect(DSN)
print "Encoding for this connection is", conn.encoding

conn.set_isolation_level(ISOLATION_LEVEL_AUTOCOMMIT)
curs = conn.cursor()

curs.execute("listen test")

print "Waiting for 'NOTIFY test'"
while 1:
    if select.select([conn],[],[],5)==([],[],[]):
        print "Timeout"
    else:
        conn.poll()
        while conn.notifies:
            print "Got NOTIFY:", conn.notifies.pop()
