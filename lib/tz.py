"""tzinfo implementations for psycopg2

This module holds two different tzinfo implementations that can be used as
the 'tzinfo' argument to datetime constructors, directly passed to psycopg
functions or used to set the .tzinfo_factory attribute in cursors. 
"""
# psycopg/tz.py - tzinfo implementation
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

import datetime
import time


ZERO = datetime.timedelta(0)

class FixedOffsetTimezone(datetime.tzinfo):
    """Fixed offset in minutes east from UTC.

    This is exactly the implementation found in Python 2.3.x documentation,
    with a small change to the __init__ method to allow for pickling and a
    default name in the form 'sHH:MM' ('s' is the sign.)
    """
    _name = None
    _offset = ZERO
    
    def __init__(self, offset=None, name=None):
        if offset is not None:
            self._offset = datetime.timedelta(minutes = offset)
        if name is not None:
            self._name = name

    def utcoffset(self, dt):
        return self._offset

    def tzname(self, dt):
        if self._name is not None:
            return self._name
        else:
            seconds = self._offset.seconds + self._offset.days * 86400
            hours, seconds = divmod(seconds, 3600)
            minutes = seconds/60
            if minutes:
                return "%+03d:%d" % (hours, minutes)
            else:
                return "%+03d" % hours
            
    def dst(self, dt):
        return ZERO


STDOFFSET = datetime.timedelta(seconds = -time.timezone)
if time.daylight:
    DSTOFFSET = datetime.timedelta(seconds = -time.altzone)
else:
    DSTOFFSET = STDOFFSET
DSTDIFF = DSTOFFSET - STDOFFSET

class LocalTimezone(datetime.tzinfo):
    """Platform idea of local timezone.

    This is the exact implementation from the Pyhton 2.3 documentation.
    """
    
    def utcoffset(self, dt):
        if self._isdst(dt):
            return DSTOFFSET
        else:
            return STDOFFSET

    def dst(self, dt):
        if self._isdst(dt):
            return DSTDIFF
        else:
            return ZERO

    def tzname(self, dt):
        return time.tzname[self._isdst(dt)]

    def _isdst(self, dt):
        tt = (dt.year, dt.month, dt.day,
              dt.hour, dt.minute, dt.second,
              dt.weekday(), 0, -1)
        stamp = time.mktime(tt)
        tt = time.localtime(stamp)
        return tt.tm_isdst > 0

LOCAL = LocalTimezone()

# TODO: pre-generate some interesting time zones?
