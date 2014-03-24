# lastrowid.py -  example of using .lastrowid attribute
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

import sys, psycopg2

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dsn:", DSN
conn = psycopg2.connect(DSN)
curs = conn.cursor()

try:
    curs.execute("CREATE TABLE test_oid (name text, surname text)")
except:
    conn.rollback()
    curs.execute("DROP TABLE test_oid")
    curs.execute("CREATE TABLE test_oid (name text, surname text)")
conn.commit()

data = ({'name':'Federico', 'surname':'Di Gregorio'},
        {'name':'Pierluigi', 'surname':'Di Nunzio'})

curs.execute("""INSERT INTO test_oid
                VALUES (%(name)s, %(surname)s)""", data[0])

foid = curs.lastrowid
print "Oid for %(name)s %(surname)s" % data[0], "is", foid

curs.execute("""INSERT INTO test_oid
                VALUES (%(name)s, %(surname)s)""", data[1])
moid = curs.lastrowid
print "Oid for %(name)s %(surname)s" % data[1], "is", moid

curs.execute("SELECT * FROM test_oid WHERE oid = %s", (foid,))
print "Oid", foid, "selected %s %s" % curs.fetchone()

curs.execute("SELECT * FROM test_oid WHERE oid = %s", (moid,))
print "Oid", moid, "selected %s %s" % curs.fetchone()

curs.execute("DROP TABLE test_oid")
conn.commit()
