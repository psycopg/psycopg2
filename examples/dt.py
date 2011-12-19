# datetime.py -  example of using date and time types
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
import mx.DateTime
import datetime

from psycopg2.extensions import adapt

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dns:", DSN
conn = psycopg2.connect(DSN)
curs = conn.cursor()

try:
    curs.execute("""CREATE TABLE test_dt (
                     k int4, d date, t time, dt timestamp, z interval)""")
except:
    conn.rollback()
    curs.execute("DROP TABLE test_dt")
    curs.execute("""CREATE TABLE test_dt (
                     k int4, d date, t time, dt timestamp, z interval)""")
conn.commit()

# build and insert some data using mx.DateTime
mx1 = (
    1,
    mx.DateTime.Date(2004, 10, 19),
    mx.DateTime.Time(0, 11, 17.015),
    mx.DateTime.Timestamp(2004, 10, 19, 0, 11, 17.5),
    mx.DateTime.DateTimeDelta(13, 15, 17, 59.9))

from psycopg2.extensions import adapt
import psycopg2.extras
print adapt(mx1)

print "Inserting mx.DateTime values..."
curs.execute("INSERT INTO test_dt VALUES (%s, %s, %s, %s, %s)", mx1)

# build and insert some values using the datetime adapters
dt1 = (
    2,
    datetime.date(2004, 10, 19),
    datetime.time(0, 11, 17, 15000),
    datetime.datetime(2004, 10, 19, 0, 11, 17, 500000),
    datetime.timedelta(13, 15*3600+17*60+59, 900000))

print "Inserting Python datetime values..."
curs.execute("INSERT INTO test_dt VALUES (%s, %s, %s, %s, %s)", dt1)

# now extract the row from database and print them
print "Extracting values inserted with mx.DateTime wrappers:"
curs.execute("SELECT d, t, dt, z FROM test_dt WHERE k = 1")
for n, x in zip(mx1[1:], curs.fetchone()):
    try:
        # this will work only if psycopg has been compiled with datetime
        # as the default typecaster for date/time values
        s = repr(n) + "\n -> " + str(adapt(n)) + \
            "\n -> " + repr(x) + "\n -> " + x.isoformat()
    except:
        s = repr(n) + "\n -> " + str(adapt(n))  + \
            "\n -> " + repr(x) + "\n -> " + str(x)
    print s
print

print "Extracting values inserted with Python datetime wrappers:"
curs.execute("SELECT d, t, dt, z FROM test_dt WHERE k = 2")
for n, x in zip(dt1[1:], curs.fetchone()):
    try:
        # this will work only if psycopg has been compiled with datetime
        # as the default typecaster for date/time values
        s = repr(n) + "\n -> " +  repr(x) + "\n -> " + x.isoformat()
    except:
        s = repr(n) + "\n -> " +  repr(x) + "\n -> " + str(x)
    print s
print

curs.execute("DROP TABLE test_dt")
conn.commit()
