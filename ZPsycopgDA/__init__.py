# ZPsycopgDA/__init__.py - ZPsycopgDA Zope product
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

__doc__ = "ZPsycopg Database Adapter Registration." 
__version__ = '2.0'

import DA

def initialize(context):
    context.registerClass(
        DA.Connection,
        permission = 'Add Z Psycopg 2 Database Connections',
        constructors = (DA.manage_addZPsycopgConnectionForm,
                        DA.manage_addZPsycopgConnection),
        icon = SOFTWARE_HOME + '/Shared/DC/ZRDB/www/DBAdapterFolder_icon.gif')
