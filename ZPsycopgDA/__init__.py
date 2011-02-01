# ZPsycopgDA/__init__.py - ZPsycopgDA Zope product
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

# Import modules needed by _psycopg to allow tools like py2exe to do
# their work without bothering about the module dependencies.

__doc__ = "ZPsycopg Database Adapter Registration." 
__version__ = '2.0'

import DA

def initialize(context):
    context.registerClass(
        DA.Connection,
        permission = 'Add Z Psycopg 2 Database Connections',
        constructors = (DA.manage_addZPsycopgConnectionForm,
                        DA.manage_addZPsycopgConnection),
        icon = 'icons/DBAdapterFolder_icon.gif')
