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
#   USE, DATA, OR PROFITS; OR BUSINESS INTERUPTION) HOWEVER CAUSED AND
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

'''$Id: db.py 532 2004-09-27 18:35:25Z fog $'''
__version__='$Revision: 1.31.2.7 $'[11:-2]


from Shared.DC.ZRDB.TM import TM
from Shared.DC.ZRDB import dbi_db

import string, sys
from string import strip, split, find
from time import time
from types import ListType
import site

import psycopg
from psycopg import NUMBER, STRING, INTEGER, FLOAT, DATETIME
from psycopg import BOOLEAN, ROWID, LONGINTEGER
from ZODB.POSException import ConflictError

class DB(TM,dbi_db.DB):

    _p_oid = _p_changed = _registered = None

    def __init__(self, connection, tilevel, enc='utf-8'):
        self.connection = connection
        self.tilevel = tilevel
        self.encoding = enc 
        self.db = self.connect(self.connection)
        self.failures = 0
        self.calls = 0

    def connect(self, connection):
        o = psycopg.connect(connection)
        o.set_isolation_level(int(self.tilevel))
        return o
    
    def _finish(self, *ignored):
        if hasattr(self, 'db') and self.db:
            self.db.commit()
            
    def _abort(self, *ignored):
        if hasattr(self, 'db') and self.db:
            self.db.rollback()
            
    def _cursor(self):
        """Obtains a cursor in a safe way."""
        if not hasattr(self, 'db') or not self.db:
            self.db = self.connect(self.connection)
        return self.db.cursor()
    
    def tables(self, rdb=0, _care=('TABLE', 'VIEW')):
        self._register()
        c = self._cursor()
        c.execute('SELECT t.tablename AS NAME, '
                  '\'TABLE\' AS TYPE FROM pg_tables t '
                  'WHERE tableowner <> \'postgres\' '
                  'UNION SELECT v.viewname AS NAME, '
                  '\'VIEW\' AS TYPE FROM pg_views v '
                  'WHERE viewowner <> \'postgres\' '
                  'UNION SELECT t.tablename AS NAME, '
                  '\'SYSTEM_TABLE\' AS TYPE FROM pg_tables t '
                  'WHERE tableowner = \'postgres\' '
                  'UNION SELECT v.viewname AS NAME, '
                  '\'SYSTEM_TABLE\' AS TYPE FROM pg_views v '
                  'WHERE viewowner = \'postgres\' ' )
        r = []
        a = r.append
        for name, typ in c.fetchall():
            if typ in _care:
                a({'TABLE_NAME': name, 'TABLE_TYPE': typ})
        c.close()
        return r

    def columns(self, table_name):
        self._register()
        c = self._cursor()
        try:
            r = c.execute('select * from "%s" where 1=0' % table_name)
        except:
            return ()
        desc = c.description
        r = []
        a = r.append
        for name, type, width, ds, p, scale, null_ok in desc:
            if type == NUMBER:
                if type == INTEGER:
                    type = INTEGER
                elif type == FLOAT:
                    type = FLOAT
                else: type = NUMBER
            elif type == BOOLEAN:
                type = BOOLEAN
            elif type == ROWID:
                type = ROWID
            elif type == DATETIME:
                type = DATETIME
            else:
                type = STRING
            a({ 'Name': name,
                'Type': type.name,
                'Precision': 0,
                'Scale': 0,
                'Nullable': 0})
        return r

    def query(self, query_string, max_rows=None, query_data=None):
        self._register()
        self.calls = self.calls+1
        
        desc = ()
        result = []
        nselects = 0
        
        c = self._cursor()
        
        try:
            for qs in filter(None, map(strip, split(query_string, '\0'))):
                if type(qs) == unicode:
                    if self.encoding:
                        qs = qs.encode(self.encoding)
                try:
                    if (query_data):
                        r = c.execute(qs, query_data)
                    else:
                        r = c.execute(qs)
                except (psycopg.ProgrammingError,psycopg.IntegrityError), perr:
                    if perr.args[0].find("concurrent update") > -1:
                        raise ConflictError
                    raise perr
                if c.description is not None:
                    nselects = nselects + 1
                    if c.description != desc and nselects > 1:
                        raise 'Query Error', \
                              'Multiple select schema are not allowed'
                    if max_rows:
                        result = c.fetchmany(max_rows)
                    else:
                        result = c.fetchall()
                    desc = c.description
            self.failures = 0
            
        except StandardError, err:
            self._abort()
            raise err
        
        items = []
        for name, typ, width, ds, p, scale, null_ok in desc:
            if typ == NUMBER:
                if typ == INTEGER or typ == LONGINTEGER: typs = 'i'
                else: typs = 'n'
            elif typ == BOOLEAN:
                typs = 'n'
            elif typ == ROWID:
                typs = 'i'
            elif typ == DATETIME:
                typs = 'd'
            else:
                typs = 's'
            items.append({
                'name': name,
                'type': typs,
                'width': width,
                'null': null_ok,
                })

        return items, result

    def close(self):
        """Close the connection."""
        self.db.close()
        self.db = None
        
    def sortKey(self):
        """Zope 2.6 added this one."""
        return 1
