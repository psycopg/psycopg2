# encoding.py - show to change client enkoding (and test it works)
# -*- encoding: utf8 -*-
#
# Copyright (C) 2004-2010 Federico Di Gregorio  <fog@debian.org>
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
import psycopg2.extensions

if len(sys.argv) > 1:
    DSN = sys.argv[1]

print "Opening connection using dns:", DSN
conn = psycopg2.connect(DSN)
print "Initial encoding for this connection is", conn.encoding

print "\n** This example is supposed to be run in a UNICODE terminal! **\n"

print "Available encodings:"
encs = psycopg2.extensions.encodings.items()
encs.sort()
for a, b in encs:
    print " ", a, "<->", b

print "Using STRING typecaster"    
print "Setting backend encoding to LATIN1 and executing queries:"
conn.set_client_encoding('LATIN1')
curs = conn.cursor()
curs.execute("SELECT %s::TEXT AS foo", ('àèìòù',))
x = curs.fetchone()[0]
print "  ->", unicode(x, 'latin-1').encode('utf-8'), type(x)
curs.execute("SELECT %s::TEXT AS foo", (u'àèìòù',))
x = curs.fetchone()[0]
print "  ->", unicode(x, 'latin-1').encode('utf-8'), type(x)

print "Setting backend encoding to UTF8 and executing queries:"
conn.set_client_encoding('UNICODE')
curs = conn.cursor()
curs.execute("SELECT %s::TEXT AS foo", (u'àèìòù'.encode('utf-8'),))
x = curs.fetchone()[0]
print "  ->", x, type(x)
curs.execute("SELECT %s::TEXT AS foo", (u'àèìòù',))
x = curs.fetchone()[0]
print "  ->", x, type(x)

print "Using UNICODE typecaster"
psycopg2.extensions.register_type(psycopg2.extensions.UNICODE)

print "Setting backend encoding to LATIN1 and executing queries:"
conn.set_client_encoding('LATIN1')
curs = conn.cursor()
curs.execute("SELECT %s::TEXT AS foo", ('àèìòù',))
x = curs.fetchone()[0]
print "  ->", x.encode('utf-8'), ":", type(x)
curs.execute("SELECT %s::TEXT AS foo", (u'àèìòù',))
x = curs.fetchone()[0]
print "  ->", x.encode('utf-8'), ":", type(x)

print "Setting backend encoding to UTF8 and executing queries:"
conn.set_client_encoding('UNICODE')
curs = conn.cursor()
curs.execute("SELECT %s::TEXT AS foo", (u'àèìòù'.encode('utf-8'),))
x = curs.fetchone()[0]
print "  ->", x.encode('utf-8'), ":", type(x)
curs.execute("SELECT %s::TEXT AS foo", (u'àèìòù',))
x = curs.fetchone()[0]
print "  ->", x.encode('utf-8'), ":", type(x)

print "Executing full UNICODE queries"

print "Setting backend encoding to LATIN1 and executing queries:"
conn.set_client_encoding('LATIN1')
curs = conn.cursor()
curs.execute(u"SELECT %s::TEXT AS foo", ('àèìòù',))
x = curs.fetchone()[0]
print "  ->", x.encode('utf-8'), ":", type(x)
curs.execute(u"SELECT %s::TEXT AS foo", (u'àèìòù',))
x = curs.fetchone()[0]
print "  ->", x.encode('utf-8'), ":", type(x)

print "Setting backend encoding to UTF8 and executing queries:"
conn.set_client_encoding('UNICODE')
curs = conn.cursor()
curs.execute(u"SELECT %s::TEXT AS foo", (u'àèìòù'.encode('utf-8'),))
x = curs.fetchone()[0]
print "  ->", x.encode('utf-8'), ":", type(x)
curs.execute(u"SELECT %s::TEXT AS foo", (u'àèìòù',))
x = curs.fetchone()[0]
print "  ->", x.encode('utf-8'), ":", type(x)
