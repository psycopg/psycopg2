"""
Using a tuple as a bound variable in "SELECT ... IN (...)" clauses
in PostgreSQL using psycopg2

Some time ago someone asked on the psycopg mailing list how to have a
bound variable expand to the right SQL for an SELECT IN clause:

    SELECT * FROM atable WHERE afield IN (value1, value2, value3)

with the values to be used in the IN clause to be passed to the cursor
.execute() method in a tuple as a bound variable, i.e.:

    in_values = ("value1", "value2", "value3")
    curs.execute("SELECT ... IN %s", (in_values,))

psycopg 1 does support typecasting from Python to PostgreSQL (and back)
only for simple types and this problem has no elegant solution (short or
writing a wrapper class returning the pre-quoted text in an __str__
method.

But psycopg2 offers a simple and elegant solution by partially
implementing the Object Adaptation from PEP 246. psycopg2 moves
the type-casting logic into external adapters and a somehow
broken adapt() function.

While the original adapt() takes 3 arguments, psycopg2's one only takes
1: the bound variable to be adapted. The result is an object supporting
a not-yet well defined protocol that we can call ISQLQuote:

    class ISQLQuote:

        def getquoted(self):
            "Returns a quoted string representing the bound variable."

        def getbinary(self):
           "Returns a binary quoted string representing the bound variable."
    
        def getbuffer(self):
            "Returns the wrapped object itself."

        __str__ = getquoted

Then one of the functions (usually .getquoted()) is called by psycopg2 at
the right time to obtain the right, sql-quoted representation for the
corresponding bound variable.

The nice part is that the default, built-in adapters, derived from
psycopg 1 tyecasting code can be overridden by the programmer, simply
replacing them in the psycopg.extensions.adapters dictionary.

Then the solution to the original problem is now obvious: write an
adapter that adapts tuple objects into the right SQL string, by calling
recursively adapt() on each element.

psycopg2 development can be tracked on the psycopg mailing list:

   http://lists.initd.org/mailman/listinfo/psycopg

"""

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

import psycopg2
import psycopg2.extensions
from psycopg2.extensions import adapt as psycoadapt
from psycopg2.extensions import register_adapter

class AsIs(object):
    """An adapter that just return the object 'as is'.

    psycopg 1.99.9 has some optimizations that make impossible to call
    adapt() without adding some basic adapters externally. This limitation
    will be lifted in a future release.
    """
    def __init__(self, obj):
        self.__obj = obj
    def getquoted(self):
        return self.__obj
    
class SQL_IN(object):
    """Adapt a tuple to an SQL quotable object."""
    
    def __init__(self, seq):
	self._seq = seq

    def prepare(self, conn):
        pass

    def getquoted(self):
        # this is the important line: note how every object in the
        # list is adapted and then how getquoted() is called on it

	qobjs = [str(psycoadapt(o).getquoted()) for o in self._seq]

	return '(' + ', '.join(qobjs) + ')'
	
    __str__ = getquoted

    
# add our new adapter class to psycopg list of adapters
register_adapter(tuple, SQL_IN)
register_adapter(float, AsIs)
register_adapter(int, AsIs)

# usually we would call:
#
#     conn = psycopg.connect("...")
#     curs = conn.cursor()
#     curs.execute("SELECT ...", (("this", "is", "the", "tuple"),))
#     
# but we have no connection to a database right now, so we just check
# the SQL_IN class by calling psycopg's adapt() directly:

if __name__ == '__main__':
    print "Note how the string will be SQL-quoted, but the number will not:"
    print psycoadapt(("this is an 'sql quoted' str\\ing", 1, 2.0))
