# lobject.py - lobject example
#
# Copyright (C) 2001-2006 Federico Di Gregorio  <fog@debian.org>
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
import psycopg2

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dns:", DSN
conn = psycopg2.connect(DSN)
print "Encoding for this connection is", conn.encoding

# this will create a large object with a new random oid, we'll
# use it to make some basic tests about read/write and seek.
lobj = conn.lobject()
loid = lobj.oid
print "Created a new large object with oid", loid

print "Manually importing some binary data into the object:"
data = open("somehackers.jpg").read()
len = lobj.write(data)
print "  imported", len, "bytes of data"

conn.commit()

print "Trying to (re)open large object with oid", loid
lobj = conn.lobject(loid)
print "Manually exporting the data from the lobject:"
data1 = lobj.read()
len = lobj.tell()
lobj.seek(0, 0)
data2 = lobj.read()
if data1 != data2:
    print "ERROR: read after seek returned different data"
open("somehackers_lobject1.jpg", 'wb').write(data1)
print "  written", len, "bytes of data to somehackers_lobject1.jpg"

lobj.unlink()
print "Large object with oid", loid, "removed"

conn.commit()

# now we try to use the import and export functions to do the same
lobj = conn.lobject(0, 'n', 0, "somehackers.jpg")
loid = lobj.oid
print "Imported a new large object with oid", loid

conn.commit()

print "Trying to (re)open large object with oid", loid
lobj = conn.lobject(loid, 'n')
print "Using export() to export the data from the large object:"
lobj.export("somehackers_lobject2.jpg")
print "  exported large object to somehackers_lobject2.jpg"

lobj.unlink()
print "Large object with oid", loid, "removed"

conn.commit()

# this will create a very large object with a new random oid.
lobj = conn.lobject()
loid = lobj.oid
print "Created a new large object with oid", loid

print "Manually importing a lot of data into the object:"
data = "data" * 1000000
len = lobj.write(data)
print "  imported", len, "bytes of data"

conn.rollback()

print "\nNow try to load the new images, to check it worked!"
