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
database_type='Psycopg'
__doc__='''%s Database Connection

$Id: DA.py 531 2004-09-18 09:54:40Z fog $''' % database_type
__version__='$Revision: 1.20.2.14 $'[11:-2]
__psycopg_versions__ = ('1.1.12', '1.1.13', '1.1.14', '1.1.15', '1.1.16')


from db import DB
import Shared.DC.ZRDB.Connection, sys, DABase, time
from Globals import HTMLFile, ImageFile
from ExtensionClass import Base
from string import find, join, split, rindex
try:
    import psycopg
    from psycopg import new_type, register_type, DATETIME, TIME, DATE, INTERVAL
except StandardError, err:
    print err
try:
    from DateTime import DateTime
except StandardError, err:
    print err
try:
    from App.Dialogs import MessageDialog
except:
    pass
import time

manage_addZPsycopgConnectionForm = HTMLFile('connectionAdd', globals())
def manage_addZPsycopgConnection(self, id, title,
                                 connection_string, zdatetime=None,
				 tilevel=2, check=None, REQUEST=None):
    """Add a DB connection to a folder"""
    self._setObject(id, Connection(id, title, connection_string, zdatetime,
                                   check, tilevel))
    if REQUEST is not None: return self.manage_main(self,REQUEST)


# Convert an ISO timestamp string from postgres to a DateTime (zope version)
# object.
def cast_DateTime(str):
    if str:
        # this will split us into [date, time, GMT/AM/PM(if there)]
        dt = split(str, ' ')
        if len(dt) > 1:
            # we now should split out any timezone info
            dt[1] = split(dt[1], '-')[0]
            dt[1] = split(dt[1], '+')[0]
            t = time.mktime(time.strptime(join(dt[:2], ' '), '%Y-%m-%d %H:%M:%S'))
        else:
            t = time.mktime(time.strptime(dt[0], '%Y-%m-%d %H:%M:%S'))
        return DateTime(t)

# Convert an ISO date string from postgres to a DateTime(zope version)
# object.
def cast_Date(str):
    if str:
        return DateTime(time.mktime(time.strptime(str, '%Y-%m-%d')))

# Convert a time string from postgres to a DateTime(zope version) object.
# WARNING: We set the day as today before feeding to DateTime so
# that it has the same DST settings.
def cast_Time(str):
    if str:
        return DateTime(time.strftime('%Y-%m-%d %H:%M:%S',
                                  time.localtime(time.time())[:3]+
                                  time.strptime(str[:8], "%H:%M:%S")[3:]))

# Convert a time string from postgres to a DateTime(zope version) object.
# WARNING: We set the day as the epoch day (1970-01-01) since this
# DateTime does not support time deltas. (EXPERIMENTAL USE WITH CARE!)
def cast_Interval(str):
    return str


class Connection(DABase.Connection):
    "The connection class."
    database_type = database_type
    id = '%s_database_connection' % database_type
    meta_type = title = 'Z %s Database Connection' % database_type
    icon = 'misc_/Z%sDA/conn' % database_type

    def __init__(self, id, title, connection_string, zdatetime,
                 check=None, tilevel=2, encoding='UTF-8'):
        self.zdatetime=zdatetime
        self.id=str(id)
        self.edit(title, connection_string, zdatetime,
                  check=check, tilevel=tilevel, encoding=encoding)

    def edit(self, title, connection_string, zdatetime,
             check=1, tilevel=2, encoding='UTF-8'):
        self.title=title
        self.connection_string=connection_string
        self.zdatetime=zdatetime
	self.tilevel=tilevel
        self.encoding=encoding
        self.set_type_casts()
        if check: self.connect(connection_string)

    manage_properties=HTMLFile('connectionEdit', globals())

    def manage_edit(self, title, connection_string,
                    zdatetime=None, check=None, tilevel=2, encoding='UTF-8',
                    REQUEST=None):
        """Change connection
        """
        self.edit(title, connection_string, zdatetime,
                  check=check, tilevel=tilevel, encoding=encoding)
        if REQUEST is not None:
            return MessageDialog(
                title='Edited',
                message='<strong>%s</strong> has been edited.' % self.id,
                action ='./manage_main',
                )
        
    def set_type_casts(self):
        "Make changes to psycopg default typecast list"
        if self.zdatetime:
            #use zope internal datetime routines
            ZDATETIME=new_type((1184,1114), "ZDATETIME", cast_DateTime)
            ZDATE=new_type((1082,), "ZDATE", cast_Date)
            ZTIME=new_type((1083,), "ZTIME", cast_Time)
            ZINTERVAL=new_type((1186,), "ZINTERVAL", cast_Interval)
            register_type(ZDATETIME)
            register_type(ZDATE)
            register_type(ZTIME)
            register_type(ZINTERVAL)
        else:
            #use the standard. WARN: order is important!
            register_type(DATETIME)
            register_type(DATE)
            register_type(TIME)
            register_type(INTERVAL)
            
    def factory(self):
        return DB

    def table_info(self):
	return self._v_database_connection.table_info()

    def connect(self,s):
        try: self._v_database_connection.close()
        except: pass

        # check psycopg version and raise exception if does not match
        if psycopg.__version__ not in __psycopg_versions__:
            raise ImportError("psycopg version mismatch: " +
                              psycopg.__version__)

        self.set_type_casts()
        self._v_connected=''
        DB=self.factory()
        try:
            try:
                # this is necessary when upgrading from old installs without
                # having to recreate the connection object
                if not hasattr(self, 'tilevel'):
                    self.tilevel = 2
                if not hasattr(self, 'encoding'):
                    self.encoding = 'UTF-8'
                self._v_database_connection=DB(s, self.tilevel, self.encoding)
            except:
                t, v, tb = sys.exc_info()
                raise 'BadRequest', (
                    '<strong>Could not open connection.<br>'
                    'Connection string: </strong><CODE>%s</CODE><br>\n'
                    '<pre>\n%s\n%s\n</pre>\n'
                    % (s,t,v)), tb
        finally: tb=None
        self._v_connected=DateTime()

        return self

    def sql_quote__(self, v):
        # quote dictionary
        quote_dict = {"\'": "''", "\\": "\\\\"}
        for dkey in quote_dict.keys():
            if find(v, dkey) >= 0:
                v=join(split(v,dkey),quote_dict[dkey])
        return "'%s'" % v


classes = ('DA.Connection',)

meta_types=(
    {'name':'Z %s Database Connection' % database_type,
     'action':'manage_addZ%sConnectionForm' % database_type},)

folder_methods={
    'manage_addZPsycopgConnection': manage_addZPsycopgConnection,
    'manage_addZPsycopgConnectionForm': manage_addZPsycopgConnectionForm}

__ac_permissions__=(
    ('Add Z Psycopg Database Connections',
     ('manage_addZPsycopgConnectionForm', 'manage_addZPsycopgConnection')),)

misc_={
    'conn': ImageFile('Shared/DC/ZRDB/www/DBAdapterFolder_icon.gif')}

for icon in ('table', 'view', 'stable', 'what',
	     'field', 'text','bin','int','float',
	     'date','time','datetime'):
    misc_[icon] = ImageFile('icons/%s.gif' % icon, globals())
