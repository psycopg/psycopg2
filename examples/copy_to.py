# copy_to.py -- example about copy_to 
#
# Copyright (C) 2002 Tom Jenkins <tjenkins@devis.com>
# Copyright (C) 2005 Federico Di Gregorio <fog@initd.org>
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
import os
import StringIO
import psycopg2

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dsn:", DSN
conn = psycopg2.connect(DSN)
print "Encoding for this connection is", conn.encoding

curs = conn.cursor()
try:
    curs.execute("CREATE TABLE test_copy (fld1 text, fld2 text, fld3 int4)")
except:
    conn.rollback()
    curs.execute("DROP TABLE test_copy")
    curs.execute("CREATE TABLE test_copy (fld1 text, fld2 text, fld3 int4)")
conn.commit()

# demostrate copy_to functionality
data = [('Tom', 'Jenkins', '37'),
        ('Madonna', None, '45'),
        ('Federico', 'Di Gregorio', None)]
query = "INSERT INTO test_copy VALUES (%s, %s, %s)"
curs.executemany(query, data)
conn.commit()

# copy_to using defaults
io = open('copy_to.txt', 'w')
curs.copy_to(io, 'test_copy')
print "1) Copy %d records into file object using defaults: " % len (data) + \
      "sep = \\t and null = \\N"
io.close()

rows = open('copy_to.txt', 'r').readlines()
print "   File has %d rows:" % len(rows)

for r in rows:
    print "   ", r,

# copy_to using custom separator
io = open('copy_to.txt', 'w')
curs.copy_to(io, 'test_copy', ':')
print "2) Copy %d records into file object using sep = :" % len(data)
io.close()

rows = open('copy_to.txt', 'r').readlines()
print "   File has %d rows:" % len(rows)

for r in rows:
    print "   ", r,

# copy_to using custom null identifier
io = open('copy_to.txt', 'w')
curs.copy_to(io, 'test_copy', null='NULL')
print "3) Copy %d records into file object using null = NULL" % len(data)
io.close()

rows = open('copy_to.txt', 'r').readlines()
print "   File has %d rows:" % len(rows)

for r in rows:
    print "   ", r,

# copy_to using custom separator and null identifier
io = open('copy_to.txt', 'w')
curs.copy_to(io, 'test_copy', ':', 'NULL')
print "4) Copy %d records into file object using sep = : and null ) NULL" % \
      len(data)
io.close()

rows = open('copy_to.txt', 'r').readlines()
print "   File has %d rows:" % len(rows)

for r in rows:
    print "   ", r,

curs.execute("DROP TABLE test_copy")
os.unlink('copy_to.txt')
conn.commit()
