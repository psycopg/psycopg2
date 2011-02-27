"""A Python driver for PostgreSQL

psycopg is a PostgreSQL_ database adapter for the Python_ programming
language. This is version 2, a complete rewrite of the original code to
provide new-style classes for connection and cursor objects and other sweet
candies. Like the original, psycopg 2 was written with the aim of being very
small and fast, and stable as a rock.

Homepage: http://initd.org/projects/psycopg2

.. _PostgreSQL: http://www.postgresql.org/
.. _Python: http://www.python.org/

:Groups:
  * `Connections creation`: connect
  * `Value objects constructors`: Binary, Date, DateFromTicks, Time,
    TimeFromTicks, Timestamp, TimestampFromTicks
"""
# psycopg/__init__.py - initialization of the psycopg module
#
# Copyright (C) 2003-2010 Federico Di Gregorio  <fog@debian.org>
#
# psycopg2 is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# In addition, as a special exception, the copyright holders give
# permission to link this program with the OpenSSL library (or with
# modified versions of OpenSSL that use the same license as OpenSSL),
# and distribute linked combinations including the two.
#
# You must obey the GNU Lesser General Public License in all respects for
# all of the code used other than OpenSSL.
#
# psycopg2 is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.

# Import modules needed by _psycopg to allow tools like py2exe to do
# their work without bothering about the module dependencies.
# 
# TODO: we should probably use the Warnings framework to signal a missing
# module instead of raising an exception (in case we're running a thin
# embedded Python or something even more devious.)

import sys, warnings
if sys.version_info[0] >= 2 and sys.version_info[1] >= 3:
    try:
        import datetime as _psycopg_needs_datetime
    except:
        warnings.warn(
            "can't import datetime module probably needed by _psycopg",
            RuntimeWarning)
if sys.version_info[0] >= 2 and sys.version_info[1] >= 4:
    try:
        import decimal as _psycopg_needs_decimal
    except:
        warnings.warn(
            "can't import decimal module probably needed by _psycopg",
            RuntimeWarning)
del sys, warnings

# Note: the first internal import should be _psycopg, otherwise the real cause
# of a failed loading of the C module may get hidden, see
# http://archives.postgresql.org/psycopg/2011-02/msg00044.php

# Import the DBAPI-2.0 stuff into top-level module.

from psycopg2._psycopg import BINARY, NUMBER, STRING, DATETIME, ROWID

from psycopg2._psycopg import Binary, Date, Time, Timestamp
from psycopg2._psycopg import DateFromTicks, TimeFromTicks, TimestampFromTicks

from psycopg2._psycopg import Error, Warning, DataError, DatabaseError, ProgrammingError
from psycopg2._psycopg import IntegrityError, InterfaceError, InternalError
from psycopg2._psycopg import NotSupportedError, OperationalError

from psycopg2._psycopg import connect, apilevel, threadsafety, paramstyle
from psycopg2._psycopg import __version__

from psycopg2 import tz


# Register default adapters.

import psycopg2.extensions as _ext
_ext.register_adapter(tuple, _ext.SQL_IN)
_ext.register_adapter(type(None), _ext.NoneAdapter)

__all__ = filter(lambda k: not k.startswith('_'), locals().keys())

