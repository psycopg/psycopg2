"""A Python driver for PostgreSQL

psycopg is a PostgreSQL database adapter for the Python programming
language. This is version 2, a complete rewrite of the original code to
provide new-style classes for connection and cursor objects and other sweet
candies. Like the original, psycopg 2 was written with the aim of being very
small and fast, and stable as a rock.
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

# import the DBAPI-2.0 stuff into top-level module
from _psycopg import BINARY, NUMBER, STRING, DATETIME, ROWID

from _psycopg import Binary, Date, Time, Timestamp
from _psycopg import DateFromTicks, TimeFromTicks, TimestampFromTicks

from _psycopg import Error, Warning, DataError, DatabaseError, ProgrammingError
from _psycopg import IntegrityError, InterfaceError, InternalError
from _psycopg import NotSupportedError, OperationalError

from _psycopg import connect, apilevel, threadsafety, paramstyle
from _psycopg import __version__
