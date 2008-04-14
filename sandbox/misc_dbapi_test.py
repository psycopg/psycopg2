#!/usr/bin/env python
"""
Test if the arguments object can be used with both positional and keyword
arguments.
"""

class O(object):

    def __init__(self, *args, **kwds):
        self.args = args
        self.kwds = kwds

    def __getitem__(self, k):
        if isinstance(k, int):
            return self.args[k]
        else:
            return self.kwds[k]

o = O('R%', second='S%')

print o[0]
print o['second']


#-------------------------------------------------------------------------------

import psycopg2 as dbapi


conn = dbapi.connect(database='test')
                    
                   

cursor = conn.cursor()
cursor.execute("""

  SELECT * FROM location_pretty
    WHERE keyname LIKE %s OR keyname LIKE %(second)s

  """, (o,))

for row in cursor:
    print row


