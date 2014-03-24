# copy_from.py -- example about copy_from 
#
# Copyright (C) 2002 Tom Jenkins <tjenkins@devis.com>
# Copyright (C) 2005 Federico Di Gregorio <fog@initd.org>
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

# copy_from with default arguments, from open file

io = open('copy_from.txt', 'wr')
data = ['Tom\tJenkins\t37\n',
        'Madonna\t\\N\t45\n',
        'Federico\tDi Gregorio\t\\N\n']
io.writelines(data)
io.close()

io = open('copy_from.txt', 'r')
curs.copy_from(io, 'test_copy')
print "1) Copy %d records from file object " % len(data) + \
      "using defaults (sep: \\t and null = \\N)"
io.close()

curs.execute("SELECT * FROM test_copy")
rows = curs.fetchall()
print "   Select returned %d rows" % len(rows)

for r in rows:
    print "    %s %s\t%s" % (r[0], r[1], r[2])
curs.execute("delete from test_copy")
conn.commit()

# copy_from using custom separator, from open file

io = open('copy_from.txt', 'wr')
data = ['Tom:Jenkins:37\n',
        'Madonna:\N:45\n',
        'Federico:Di Gregorio:\N\n']
io.writelines(data)
io.close()

io = open('copy_from.txt', 'r')
curs.copy_from(io, 'test_copy', ':')
print "2) Copy %d records from file object using sep = :" % len(data)
io.close()

curs.execute("SELECT * FROM test_copy")
rows = curs.fetchall()
print "   Select returned %d rows" % len(rows)

for r in rows:
    print "    %s %s\t%s" % (r[0], r[1], r[2])
curs.execute("delete from test_copy")
conn.commit()

# copy_from using custom null identifier, from open file

io = open('copy_from.txt', 'wr')
data = ['Tom\tJenkins\t37\n',
        'Madonna\tNULL\t45\n',
        'Federico\tDi Gregorio\tNULL\n']
io.writelines(data)
io.close()

io = open('copy_from.txt', 'r')
curs.copy_from(io, 'test_copy', null='NULL')
print "3) Copy %d records from file object using null = NULL" % len(data)
io.close()

curs.execute("SELECT * FROM test_copy")
rows = curs.fetchall()
print "   Select using cursor returned %d rows" % len(rows)

for r in rows:
    print "    %s %s\t%s" % (r[0], r[1], r[2])
curs.execute("delete from test_copy")
conn.commit()

# copy_from using custom separator and null identifier

io = open('copy_from.txt', 'wr')
data = ['Tom:Jenkins:37\n', 'Madonna:NULL:45\n', 'Federico:Di Gregorio:NULL\n']
io.writelines(data)
io.close()

io = open('copy_from.txt', 'r')
curs.copy_from(io, 'test_copy', ':', 'NULL')
print "4) Copy %d records from file object " % len(data) + \
      "using sep = : and null = NULL"
io.close()

curs.execute("SELECT * FROM test_copy")
rows = curs.fetchall()
print "   Select using cursor returned %d rows" % len(rows)

for r in rows:
    print "    %s %s\t%s" % (r[0], r[1], r[2])
curs.execute("delete from test_copy")
conn.commit()

# anything can be used as a file if it has .read() and .readline() methods

data = StringIO.StringIO()
data.write('\n'.join(['Tom\tJenkins\t37',
                      'Madonna\t\N\t45',
                      'Federico\tDi Gregorio\t\N']))
data.seek(0)

curs.copy_from(data, 'test_copy')
print "5) Copy 3 records from StringIO object using defaults"

curs.execute("SELECT * FROM test_copy")
rows = curs.fetchall()
print "   Select using cursor returned %d rows" % len(rows)

for r in rows:
    print "    %s %s\t%s" % (r[0], r[1], r[2])
curs.execute("delete from test_copy")
conn.commit()

# simple error test

print "6) About to raise an error"
data = StringIO.StringIO()
data.write('\n'.join(['Tom\tJenkins\t37',
                      'Madonna\t\N\t45',
                      'Federico\tDi Gregorio\taaa']))
data.seek(0)

try:
    curs.copy_from(data, 'test_copy')
except StandardError, err:
    conn.rollback()
    print "   Caught error (as expected):\n", err

conn.rollback()

curs.execute("DROP TABLE test_copy")
os.unlink('copy_from.txt')
conn.commit()



