# notify.py - example of getting notifies
#
# Copyright (C) 2001-2005 Federico Di Gregorio  <fog@debian.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.

## put in DSN your DSN string

DSN = 'dbname=test'

## don't modify anything below tis line (except for experimenting)

import sys
import psycopg2
import select

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dns:", DSN
conn = psycopg2.connect(DSN)
print "Encoding for this connection is", conn.encoding

conn.set_isolation_level(0)
curs = conn.cursor()

curs.execute("listen test")

print "Waiting for 'NOTIFY test'"
while 1:
    if select.select([curs],[],[],5)==([],[],[]):
        print "Timeout"
    else:
        if curs.isready():
            print "Got NOTIFY: %s" % str(curs.connection.notifies.pop()) 
