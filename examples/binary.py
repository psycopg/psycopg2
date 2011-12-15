# binary.py - working with binary data
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
    curs.execute("CREATE TABLE test_binary (id int4, name text, img bytea)")
except:
    conn.rollback()
    curs.execute("DROP TABLE test_binary")
    curs.execute("CREATE TABLE test_binary (id int4, name text, img bytea)")
conn.commit()

# first we try two inserts, one with an explicit Binary call and the other
# using a buffer on a file object.

data1 = {'id':1, 'name':'somehackers.jpg',
         'img':psycopg2.Binary(open('somehackers.jpg').read())}
data2 = {'id':2, 'name':'whereareyou.jpg',
         'img':buffer(open('whereareyou.jpg').read())}

curs.execute("""INSERT INTO test_binary
                  VALUES (%(id)s, %(name)s, %(img)s)""", data1)
curs.execute("""INSERT INTO test_binary
                  VALUES (%(id)s, %(name)s, %(img)s)""", data2)

# now we try to extract the images as simple text strings

print "Extracting the images as strings..."
curs.execute("SELECT * FROM test_binary")

for row in curs.fetchall():
    name, ext = row[1].split('.')
    new_name = name + '_S.' + ext
    print "  writing %s to %s ..." % (name+'.'+ext, new_name),
    open(new_name, 'wb').write(row[2])
    print "done"
    print "  python type of image data is", type(row[2])
    
# extract exactly the same data but using a binary cursor

print "Extracting the images using a binary cursor:"

curs.execute("""DECLARE zot CURSOR FOR
                  SELECT img, name FROM test_binary FOR READ ONLY""")
curs.execute("""FETCH ALL FROM zot""")

for row in curs.fetchall():
    name, ext = row[1].split('.')
    new_name = name + '_B.' + ext
    print "  writing %s to %s ..." % (name+'.'+ext, new_name),
    open(new_name, 'wb').write(row[0])
    print "done"
    print "  python type of image data is", type(row[0])
    
# this rollback is required because we can't drop a table with a binary cusor
# declared and still open
conn.rollback()

curs.execute("DROP TABLE test_binary")
conn.commit()

print "\nNow try to load the new images, to check it worked!"
