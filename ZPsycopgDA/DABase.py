# ZPsycopgDA/DABase.py - ZPsycopgDA Zope product: Database inspection
#
# Copyright (C) 2004 Federico Di Gregorio <fog@initd.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any later
# version.
#
# Or, at your option this program (ZPsycopgDA) can be distributed under the
# Zope Public License (ZPL) Version 1.0, as published on the Zope web site,
# http://www.zope.org/Resources/ZPL.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY
# or FITNESS FOR A PARTICULAR PURPOSE.
#
# See the LICENSE file for details.

import sys
import Shared.DC.ZRDB.Connection

from db import DB
from Globals import HTMLFile
from ImageFile import ImageFile
from ExtensionClass import Base
from DateTime import DateTime

# import psycopg and functions/singletons needed for date/time conversions

import psycopg
from psycopg.extensions import INTEGER, LONGINTEGER, FLOAT, BOOLEAN
from psycopg import NUMBER, STRING, ROWID, DATETIME 



class Connection(Shared.DC.ZRDB.Connection.Connection):
    _isAnSQLConnection = 1

    info = None
    
    #manage_options = Shared.DC.ZRDB.Connection.Connection.manage_options + (
    #    {'label': 'Browse', 'action':'manage_browse'},)

    #manage_tables = HTMLFile('tables', globals())
    #manage_browse = HTMLFile('browse',globals())

    def __getitem__(self, name):
        if name == 'tableNamed':
            if not hasattr(self, '_v_tables'): self.tpValues()
            return self._v_tables.__of__(self)
        raise KeyError, name

    
    ## old stuff from ZPsycopgDA 1.1 (never implemented) ##

    def manage_wizard(self, tables):
        "Wizard of what? Oozing?"

    def manage_join(self, tables, select_cols, join_cols, REQUEST=None):
        """Create an SQL join"""

    def manage_insert(self, table, cols, REQUEST=None):
        """Create an SQL insert"""

    def manage_update(self, table, keys, cols, REQUEST=None):
        """Create an SQL update"""
