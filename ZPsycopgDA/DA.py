# ZPsycopgDA/DA.py - ZPsycopgDA Zope product: Database Connection
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


ALLOWED_PSYCOPG_VERSIONS = ('2.4', '2.4.1', '2.4.4', '2.4.5', '2.4.6')

import sys
import time
import db
import re

import Acquisition
import Shared.DC.ZRDB.Connection

from db import DB
from Globals import HTMLFile
from ExtensionClass import Base
from App.Dialogs import MessageDialog
from DateTime import DateTime

# ImageFile is deprecated in Zope >= 2.9
try:
    from App.ImageFile import ImageFile
except ImportError:
    # Zope < 2.9.  If PIL's installed with a .pth file, we're probably
    # hosed.
    from ImageFile import ImageFile

# import psycopg and functions/singletons needed for date/time conversions

import psycopg2
from psycopg2 import NUMBER, STRING, ROWID, DATETIME
from psycopg2.extensions import INTEGER, LONGINTEGER, FLOAT, BOOLEAN, DATE
from psycopg2.extensions import TIME, INTERVAL
from psycopg2.extensions import new_type, register_type


# add a new connection to a folder

manage_addZPsycopgConnectionForm = HTMLFile('dtml/add',globals())

def manage_addZPsycopgConnection(self, id, title, connection_string,
                                 zdatetime=None, tilevel=2,
                                 encoding='', check=None, REQUEST=None):
    """Add a DB connection to a folder."""
    self._setObject(id, Connection(id, title, connection_string,
                                   zdatetime, check, tilevel, encoding))
    if REQUEST is not None: return self.manage_main(self, REQUEST)


# the connection object

class Connection(Shared.DC.ZRDB.Connection.Connection):
    """ZPsycopg Connection."""
    _isAnSQLConnection = 1
    
    id                = 'Psycopg2_database_connection' 
    database_type     = 'Psycopg2'
    meta_type = title = 'Z Psycopg 2 Database Connection'
    icon              = 'misc_/conn'

    def __init__(self, id, title, connection_string,
                 zdatetime, check=None, tilevel=2, encoding='UTF-8'):
        self.zdatetime = zdatetime
        self.id = str(id)
        self.edit(title, connection_string, zdatetime,
                  check=check, tilevel=tilevel, encoding=encoding)
        
    def factory(self):
        return DB

    ## connection parameters editing ##
    
    def edit(self, title, connection_string,
             zdatetime, check=None, tilevel=2, encoding='UTF-8'):
        self.title = title
        self.connection_string = connection_string
        self.zdatetime = zdatetime
        self.tilevel = tilevel
        self.encoding = encoding
        
        if check: self.connect(self.connection_string)

    manage_properties = HTMLFile('dtml/edit', globals())

    def manage_edit(self, title, connection_string,
                    zdatetime=None, check=None, tilevel=2, encoding='UTF-8',
                    REQUEST=None):
        """Edit the DB connection."""
        self.edit(title, connection_string, zdatetime,
                  check=check, tilevel=tilevel, encoding=encoding)
        if REQUEST is not None:
            msg = "Connection edited."
            return self.manage_main(self,REQUEST,manage_tabs_message=msg)

    def connect(self, s):
        try:
            self._v_database_connection.close()
        except:
            pass

        # check psycopg version and raise exception if does not match
        if psycopg2.__version__.split(' ')[0] not in ALLOWED_PSYCOPG_VERSIONS:
            raise ImportError("psycopg version mismatch (imported %s)" %
                              psycopg2.__version__)

        self._v_connected = ''
        dbf = self.factory()
        
        # TODO: let the psycopg exception propagate, or not?
        self._v_database_connection = dbf(
            self.connection_string, self.tilevel, self.get_type_casts(), self.encoding)
        self._v_database_connection.open()
        self._v_connected = DateTime()

        return self

    def get_type_casts(self):
        # note that in both cases order *is* important
        if self.zdatetime:
            return ZDATETIME, ZDATE, ZTIME
        else:
            return DATETIME, DATE, TIME

    ## browsing and table/column management ##

    manage_options = Shared.DC.ZRDB.Connection.Connection.manage_options
    # + (
    #    {'label': 'Browse', 'action':'manage_browse'},)

    #manage_tables = HTMLFile('dtml/tables', globals())
    #manage_browse = HTMLFile('dtml/browse', globals())

    info = None
    
    def table_info(self):
        return self._v_database_connection.table_info()


    def __getitem__(self, name):
        if name == 'tableNamed':
            if not hasattr(self, '_v_tables'): self.tpValues()
            return self._v_tables.__of__(self)
        raise KeyError, name

    def tpValues(self):
        res = []
        conn = self._v_database_connection
        for d in conn.tables(rdb=0):
            try:
                name = d['TABLE_NAME']
                b = TableBrowser()
                b.__name__ = name
                b._d = d
                b._c = c
                try:
                    b.icon = table_icons[d['TABLE_TYPE']]
                except:
                    pass
                r.append(b)
            except:
                pass
        return res


## database connection registration data ##

classes = (Connection,)

meta_types = ({'name':'Z Psycopg 2 Database Connection',
               'action':'manage_addZPsycopgConnectionForm'},)

folder_methods = {
    'manage_addZPsycopgConnection': manage_addZPsycopgConnection,
    'manage_addZPsycopgConnectionForm': manage_addZPsycopgConnectionForm}

__ac_permissions__ = (
    ('Add Z Psycopg Database Connections',
     ('manage_addZPsycopgConnectionForm', 'manage_addZPsycopgConnection')),)

# add icons

misc_={'conn': ImageFile('icons/DBAdapterFolder_icon.gif', globals())}

for icon in ('table', 'view', 'stable', 'what', 'field', 'text', 'bin',
             'int', 'float', 'date', 'time', 'datetime'):
    misc_[icon] = ImageFile('icons/%s.gif' % icon, globals())


## zope-specific psycopg typecasters ##

# convert an ISO timestamp string from postgres to a Zope DateTime object
def _cast_DateTime(iso, curs):
    if iso:
        if iso in ['-infinity', 'infinity']:
            return iso
        else:
            return DateTime(iso)

# convert an ISO date string from postgres to a Zope DateTime object
def _cast_Date(iso, curs):
    if iso:
        if iso in ['-infinity', 'infinity']:
            return iso
        else:
            return DateTime(iso)

# Convert a time string from postgres to a Zope DateTime object.
# NOTE: we set the day as today before feeding to DateTime so
# that it has the same DST settings.
def _cast_Time(iso, curs):
    if iso:
        if iso in ['-infinity', 'infinity']:
            return iso
        else:
            return DateTime(time.strftime('%Y-%m-%d %H:%M:%S',
                                      time.localtime(time.time())[:3]+
                                      time.strptime(iso[:8], "%H:%M:%S")[3:]))

# NOTE: we don't cast intervals anymore because they are passed
# untouched to Zope.
def _cast_Interval(iso, curs):
    return iso

ZDATETIME = new_type((1184, 1114), "ZDATETIME", _cast_DateTime)
ZINTERVAL = new_type((1186,), "ZINTERVAL", _cast_Interval)
ZDATE = new_type((1082,), "ZDATE", _cast_Date)
ZTIME = new_type((1083,), "ZTIME", _cast_Time)


## table browsing helpers ##

class TableBrowserCollection(Acquisition.Implicit):
    pass

class Browser(Base):
    def __getattr__(self, name):
        try:
            return self._d[name]
        except KeyError:
            raise AttributeError, name

class values:
    def len(self):
        return 1

    def __getitem__(self, i):
        try:
            return self._d[i]
        except AttributeError:
            pass
        self._d = self._f()
        return self._d[i]

class TableBrowser(Browser, Acquisition.Implicit):
    icon = 'what'
    Description = check = ''
    info = HTMLFile('table_info', globals())
    menu = HTMLFile('table_menu', globals())

    def tpValues(self):
        v = values()
        v._f = self.tpValues_
        return v

    def tpValues_(self):
        r=[]
        tname=self.__name__
        for d in self._c.columns(tname):
            b=ColumnBrowser()
            b._d=d
            try: b.icon=field_icons[d['Type']]
            except: pass
            b.TABLE_NAME=tname
            r.append(b)
        return r
            
    def tpId(self): return self._d['TABLE_NAME']
    def tpURL(self): return "Table/%s" % self._d['TABLE_NAME']
    def Name(self): return self._d['TABLE_NAME']
    def Type(self): return self._d['TABLE_TYPE']

    manage_designInput=HTMLFile('designInput',globals())
    def manage_buildInput(self, id, source, default, REQUEST=None):
        "Create a database method for an input form"
        args=[]
        values=[]
        names=[]
        columns=self._columns
        for i in range(len(source)):
            s=source[i]
            if s=='Null': continue
            c=columns[i]
            d=default[i]
            t=c['Type']
            n=c['Name']
            names.append(n)
            if s=='Argument':
                values.append("<dtml-sqlvar %s type=%s>'" %
                              (n, vartype(t)))
                a='%s%s' % (n, boboType(t))
                if d: a="%s=%s" % (a,d)
                args.append(a)
            elif s=='Property':
                values.append("<dtml-sqlvar %s type=%s>'" %
                              (n, vartype(t)))
            else:
                if isStringType(t):
                    if find(d,"\'") >= 0: d=join(split(d,"\'"),"''")
                    values.append("'%s'" % d)
                elif d:
                    values.append(str(d))
                else:
                    raise ValueError, (
                        'no default was given for <em>%s</em>' % n)

class ColumnBrowser(Browser):
    icon='field'

    def check(self):
        return ('\t<input type=checkbox name="%s.%s">' %
                (self.TABLE_NAME, self._d['Name']))
    def tpId(self): return self._d['Name']
    def tpURL(self): return "Column/%s" % self._d['Name']
    def Description(self):
        d=self._d
        if d['Scale']:
            return " %(Type)s(%(Precision)s,%(Scale)s) %(Nullable)s" % d
        else:
            return " %(Type)s(%(Precision)s) %(Nullable)s" % d

table_icons={
    'TABLE': 'table',
    'VIEW':'view',
    'SYSTEM_TABLE': 'stable',
    }

field_icons={
    NUMBER.name: 'i',
    STRING.name: 'text',
    DATETIME.name: 'date',
    INTEGER.name: 'int',
    FLOAT.name: 'float',
    BOOLEAN.name: 'bin',
    ROWID.name: 'int'
    }
