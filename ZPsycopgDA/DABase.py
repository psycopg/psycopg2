##############################################################################
# 
# Zope Public License (ZPL) Version 1.0
# -------------------------------------
# 
# Copyright (c) Digital Creations.  All rights reserved.
# 
# This license has been certified as Open Source(tm).
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
# 1. Redistributions in source code must retain the above copyright
#    notice, this list of conditions, and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions, and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 
# 3. Digital Creations requests that attribution be given to Zope
#    in any manner possible. Zope includes a "Powered by Zope"
#    button that is installed by default. While it is not a license
#    violation to remove this button, it is requested that the
#    attribution remain. A significant investment has been put
#    into Zope, and this effort will continue if the Zope community
#    continues to grow. This is one way to assure that growth.
# 
# 4. All advertising materials and documentation mentioning
#    features derived from or use of this software must display
#    the following acknowledgement:
# 
#      "This product includes software developed by Digital Creations
#      for use in the Z Object Publishing Environment
#      (http://www.zope.org/)."
# 
#    In the event that the product being advertised includes an
#    intact Zope distribution (with copyright and license included)
#    then this clause is waived.
# 
# 5. Names associated with Zope or Digital Creations must not be used to
#    endorse or promote products derived from this software without
#    prior written permission from Digital Creations.
# 
# 6. Modified redistributions of any form whatsoever must retain
#    the following acknowledgment:
# 
#      "This product includes software developed by Digital Creations
#      for use in the Z Object Publishing Environment
#      (http://www.zope.org/)."
# 
#    Intact (re-)distributions of any official Zope release do not
#    require an external acknowledgement.
# 
# 7. Modifications are encouraged but must be packaged separately as
#    patches to official Zope releases.  Distributions that do not
#    clearly separate the patches from the original work must be clearly
#    labeled as unofficial distributions.  Modifications which do not
#    carry the name Zope may be packaged in any form, as long as they
#    conform to all of the clauses above.
# 
# 
# Disclaimer
# 
#   THIS SOFTWARE IS PROVIDED BY DIGITAL CREATIONS ``AS IS'' AND ANY
#   EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#   PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DIGITAL CREATIONS OR ITS
#   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
#   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
#   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#   SUCH DAMAGE.
# 
# 
# This software consists of contributions made by Digital Creations and
# many individuals on behalf of Digital Creations.  Specific
# attributions are listed in the accompanying credits file.
# 
##############################################################################
__doc__='''Database Connection

$Id: DABase.py 400 2003-01-20 14:39:34Z fog $'''
__version__='$Revision: 1.10 $'[11:-2]


import Shared.DC.ZRDB.Connection, sys
from App.Dialogs import MessageDialog
from Globals import HTMLFile, ImageFile
from ExtensionClass import Base
import Acquisition
from psycopg import NUMBER, ROWID, STRING, INTEGER, FLOAT
from psycopg import BOOLEAN, DATETIME, LONGINTEGER

class Connection(Shared.DC.ZRDB.Connection.Connection):
    _isAnSQLConnection = 1

    
    manage_options=Shared.DC.ZRDB.Connection.Connection.manage_options+(
        {'label': 'Browse', 'action':'manage_browse'},
        # {'label': 'Design', 'action':'manage_tables'}
        )

    manage_tables = HTMLFile('tables',globals())
    manage_browse = HTMLFile('browse',globals())

    info=None

    def tpValues(self):
        #if hasattr(self, '_v_tpValues'): return self._v_tpValues
        r=[]
        # self._v_tables=tables=TableBrowserCollection()
        #tables=tables.__dict__
        c = self._v_database_connection
        try:
            for d in c.tables(rdb=0):
                try:
                    name=d['TABLE_NAME']
                    b=TableBrowser()
                    b.__name__=name
                    b._d=d
                    b._c=c
                    # b._columns=c.columns(name)
                    try: b.icon=table_icons[d['TABLE_TYPE']]
                    except: pass
                    r.append(b)
                    # tables[name]=b
                except:
                    # print d['TABLE_NAME'], sys.exc_type, sys.exc_value
                    pass

        finally: pass #print sys.exc_type, sys.exc_value
        #self._v_tpValues=r
        return r

    def __getitem__(self, name):
        if name=='tableNamed':
            if not hasattr(self, '_v_tables'): self.tpValues()
            return self._v_tables.__of__(self)
        raise KeyError, name

    def manage_wizard(self, tables):
        " "

    def manage_join(self, tables, select_cols, join_cols, REQUEST=None):
        """Create an SQL join"""

    def manage_insert(self, table, cols, REQUEST=None):
        """Create an SQL insert"""

    def manage_update(self, table, keys, cols, REQUEST=None):
        """Create an SQL update"""

class TableBrowserCollection(Acquisition.Implicit):
    "Helper class for accessing tables via URLs"
    pass

class Browser(Base):
    def __getattr__(self, name):
        try: return self._d[name]
        except KeyError: raise AttributeError, name

class values:

    def len(self): return 1

    def __getitem__(self, i):
        try: return self._d[i]
        except AttributeError:
            pass
        self._d=self._f()
        return self._d[i]

class TableBrowser(Browser, Acquisition.Implicit):
    icon='what'
    Description=check=''
    info=HTMLFile('table_info',globals())
    menu=HTMLFile('table_menu',globals())

    def tpValues(self):
        v=values()
        v._f=self.tpValues_
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
    NUMBER.name: 'int',
    STRING.name: 'text',
    DATETIME.name: 'date',
    INTEGER.name: 'int',
    FLOAT.name: 'float',
    BOOLEAN.name: 'bin',
    ROWID.name: 'int'
    }
