# -*- python -*-
#
# Copyright (C) 2001-2003 Federico Di Gregorio <fog@debian.org>
#
# This file is part of the psycopg module.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

# this a little script that analyze a file with (TYPE, NUMBER) tuples
# and write out C code ready for inclusion in psycopg. the generated
# code defines the DBAPITypeObject fundamental types and warns for
# undefined types.

import sys, os, string, copy
from string import split, join, strip


# here is the list of the foundamental types we want to import from
# postgresql header files

basic_types = (['NUMBER', ['INT8', 'INT4', 'INT2', 'FLOAT8', 'FLOAT4',
                           'NUMERIC']],
               ['LONGINTEGER', ['INT8']],
               ['INTEGER', ['INT4', 'INT2']],
               ['FLOAT', ['FLOAT8', 'FLOAT4']],
               ['DECIMAL', ['NUMERIC']],
               ['UNICODE', ['NAME', 'CHAR', 'TEXT', 'BPCHAR',
                            'VARCHAR']],
               ['STRING', ['NAME', 'CHAR', 'TEXT', 'BPCHAR',
                           'VARCHAR']],
               ['BOOLEAN', ['BOOL']],
               ['DATETIME', ['TIMESTAMP', 'TIMESTAMPTZ', 
                             'TINTERVAL', 'INTERVAL']],
               ['TIME', ['TIME', 'TIMETZ']],
               ['DATE', ['DATE']],
               ['INTERVAL', ['TINTERVAL', 'INTERVAL']],
               ['BINARY', ['BYTEA']],
               ['ROWID', ['OID']])

# unfortunately we don't have a nice way to extract array information
# from postgresql headers; we'll have to do it hard-coding :/
array_types = (['LONGINTEGER', [1016]],
               ['INTEGER', [1005, 1006, 1007]],
               ['FLOAT', [1017, 1021, 1022]],
               ['DECIMAL', [1231]],
               ['UNICODE', [1002, 1003, 1009, 1014, 1015]],
               ['STRING', [1002, 1003, 1009, 1014, 1015]],
               ['BOOLEAN', [1000]],
               ['DATETIME', [1115, 1185]],
               ['TIME', [1183, 1270]],
               ['DATE', [1182]],
               ['INTERVAL', [1187]],
               ['BINARY', [1001]],
               ['ROWID', [1028, 1013]])

# this is the header used to compile the data in the C module
HEADER = """
typecastObject_initlist typecast_builtins[] = {
"""

# then comes the footer
FOOTER = """    {NULL, NULL, NULL, NULL}\n};\n"""


# usefull error reporting function
def error(msg):
    """Report an error on stderr."""
    sys.stderr.write(msg+'\n')
    

# read couples from stdin and build list
read_types = []
for l in sys.stdin.readlines():
    oid, val = split(l)
    read_types.append((strip(oid)[:-3], strip(val)))

# look for the wanted types in the read touples
found_types = {}

for t in basic_types:
    k = t[0]
    found_types[k] = []
    for v in t[1]:
        found = filter(lambda x, y=v: x[0] == y, read_types)
        if len(found) == 0:
            error(v+': value not found')
        elif len(found) > 1:
            error(v+': too many values')
        else:
            found_types[k].append(int(found[0][1]))

# now outputs to stdout the right C-style definitions
stypes = "" ; sstruct = ""
for t in basic_types:
    k = t[0]
    s = str(found_types[k])
    s = '{' + s[1:-1] + ', 0}'
    stypes = stypes + ('static long int typecast_%s_types[] = %s;\n' % (k, s))
    sstruct += ('  {"%s", typecast_%s_types, typecast_%s_cast, NULL},\n'
                % (k, k, k))
for t in array_types:
    kt = t[0]
    ka = t[0]+'ARRAY'
    s = str(t[1])
    s = '{' + s[1:-1] + ', 0}'
    stypes = stypes + ('static long int typecast_%s_types[] = %s;\n' % (ka, s))
    sstruct += ('  {"%s", typecast_%s_types, typecast_%s_cast, "%s"},\n'
                % (ka, ka, ka, kt))
sstruct = HEADER + sstruct + FOOTER

print stypes
print sstruct
