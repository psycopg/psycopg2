"""psycopg extensions to the DBAPI-2.0

This module holds all the extensions to the DBAPI-2.0 provided by psycopg.

- `connection` -- the new-type inheritable connection class
- `cursor` -- the new-type inheritable cursor class
- `adapt()` -- exposes the PEP-246_ compatible adapting mechanism used
  by psycopg to adapt Python types to PostgreSQL ones
  
.. _PEP-246: http://www.python.org/peps/pep-0246.html
"""
# psycopg/extensions.py - DBAPI-2.0 extensions specific to psycopg
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

from _psycopg import UNICODE, INTEGER, LONGINTEGER, BOOLEAN, FLOAT
from _psycopg import TIME, DATE, INTERVAL

from _psycopg import Boolean, QuotedString, AsIs
try:
    from _psycopg import DateFromMx, TimeFromMx, TimestampFromMx
    from _psycopg import IntervalFromMx
except:
    pass
try:
    from _psycopg import DateFromPy, TimeFromPy, TimestampFromPy
    from _psycopg import IntervalFromPy
except:
    pass

from _psycopg import adapt, adapters, encodings, connection, cursor
from _psycopg import string_types, binary_types, new_type, register_type
from _psycopg import ISQLQuote

"""Isolation level values."""
ISOLATION_LEVEL_AUTOCOMMIT     = 0
ISOLATION_LEVEL_READ_COMMITTED = 1 
ISOLATION_LEVEL_SERIALIZABLE   = 2

# PostgreSQL maps the the other standard values to already defined levels
ISOLATION_LEVEL_REPEATABLE_READ  = ISOLATION_LEVEL_SERIALIZABLE
ISOLATION_LEVEL_READ_UNCOMMITTED = ISOLATION_LEVEL_READ_COMMITTED

"""Transaction status values."""
STATUS_SETUP = 0
STATUS_READY    = 1
STATUS_BEGIN    = 2
STATUS_SYNC     = 3
STATUS_ASYNC    = 4

# This is a usefull mnemonic to check if the connection is in a transaction
STATUS_IN_TRANSACTION = STATUS_BEGIN


def register_adapter(typ, callable):
    """Register 'callable' as an ISQLQuote adapter for type 'typ'."""
    adapters[(typ, ISQLQuote)] = callable

__all__ = [ k for k in locals().keys() if not k.startswith('_') ]
