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
# Copyright (C) 2003-2004 Federico Di Gregorio  <fog@debian.org>
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
from psycopg2 import tz
del sys, warnings

# Import the DBAPI-2.0 stuff into top-level module.

from _psycopg import BINARY, NUMBER, STRING, DATETIME, ROWID

from _psycopg import Binary, Date, Time, Timestamp
from _psycopg import DateFromTicks, TimeFromTicks, TimestampFromTicks

from _psycopg import Error, Warning, DataError, DatabaseError, ProgrammingError
from _psycopg import IntegrityError, InterfaceError, InternalError
from _psycopg import NotSupportedError, OperationalError

from _psycopg import connect, apilevel, threadsafety, paramstyle
from _psycopg import __version__

__all__ = [ k for k in locals().keys() if not k.startswith('_') ]
