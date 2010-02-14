# Copyright (C) 2006 Fabio Tranchitella <fabio@tranchitella.it>
#
# psycopg2da is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# In addition, as a special exception, the copyright holders give
# permission to link this program with the OpenSSL library (or with
# modified versions of OpenSSL that use the same license as OpenSSL),
# and distribute linked combinations including the two.
#
# You must obey the GNU Lesser General Public License in all respects for
# all of the code used other than OpenSSL.
#
# psycopg2da is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
#
# If you prefer you can use this package using the ZPL license as
# published on the Zope web site, http://www.zope.org/Resources/ZPL.

"""Unit tests for Psycopg2DA."""

from unittest import TestCase, TestSuite, main, makeSuite
from datetime import tzinfo, timedelta

import psycopg2
import psycopg2.extensions


class Stub(object):

    def __init__(self, **kw):
        self.__dict__.update(kw)

class TZStub(tzinfo):

    def __init__(self, h, m):
        self.offset = h * 60 + m

    def utcoffset(self, dt):
        return timedelta(minutes=self.offset)

    def dst(self, dt):
        return 0

    def tzname(self, dt):
        return ''

    def __repr__(self):
        return 'tzinfo(%d)' % self.offset

    def __reduce__(self):
        return type(self), (), self.__dict__

class ConnectionStub(object):

    def set_isolation_level(self, level):
        pass

class Psycopg2Stub(object):

    __shared_state = {}     # 'Borg' design pattern

    DATE = psycopg2.extensions.DATE
    TIME = psycopg2.extensions.TIME
    DATETIME = psycopg2.DATETIME
    INTERVAL = psycopg2.extensions.INTERVAL
    ISOLATION_LEVEL_SERIALIZABLE = psycopg2.extensions.ISOLATION_LEVEL_SERIALIZABLE

    def __init__(self):
        self.__dict__ = self.__shared_state
        self.types = {}

    def connect(self, connection_string):
        self.last_connection_string = connection_string
        return ConnectionStub()

    def new_type(self, values, name, converter):
        return Stub(name=name, values=values)

    def register_type(self, type):
        for typeid in type.values:
            self.types[typeid] = type

    def getExtensions(self):
        return self
    extensions = property(getExtensions)


class TestPsycopg2TypeConversion(TestCase):

    def test_conv_date(self):
        from psycopg2da.adapter import _conv_date
        from datetime import date
        def c(s):
            return _conv_date(s, None)
        self.assertEquals(c(''), None)
        self.assertEquals(c('2001-03-02'), date(2001, 3, 2))

    def test_conv_time(self):
        from psycopg2da.adapter import _conv_time
        from datetime import time
        def c(s):
            return _conv_time(s, None)
        self.assertEquals(c(''), None)
        self.assertEquals(c('23:17:57'), time(23, 17, 57))
        self.assertEquals(c('23:17:57.037'), time(23, 17, 57, 37000))

    def test_conv_timetz(self):
        from psycopg2da.adapter import _conv_timetz
        from datetime import time
        def c(s):
            return _conv_timetz(s, None)
        self.assertEquals(c(''), None)
        self.assertEquals(c('12:44:01+01:00'), time(12, 44, 01, 0, TZStub(1,0)))
        self.assertEquals(c('12:44:01.037-00:30'), time(12, 44, 01, 37000, TZStub(0,-30)))

    def test_conv_timestamp(self):
        from psycopg2da.adapter import _conv_timestamp
        from datetime import datetime
        def c(s):
            return _conv_timestamp(s, None)
        self.assertEquals(c(''), None)
        self.assertEquals(c('2001-03-02 12:44:01'),
                          datetime(2001, 3, 2, 12, 44, 01))
        self.assertEquals(c('2001-03-02 12:44:01.037'),
                          datetime(2001, 3, 2, 12, 44, 01, 37000))
        self.assertEquals(c('2001-03-02 12:44:01.000001'),
                          datetime(2001, 3, 2, 12, 44, 01, 1))

    def test_conv_timestamptz(self):
        from psycopg2da.adapter import _conv_timestamptz
        from datetime import datetime
        def c(s):
            return _conv_timestamptz(s, None)
        self.assertEquals(c(''), None)

        self.assertEquals(c('2001-03-02 12:44:01+01:00'),
                  datetime(2001, 3, 2, 12, 44, 01, 0, TZStub(1,0)))
        self.assertEquals(c('2001-03-02 12:44:01.037-00:30'),
                  datetime(2001, 3, 2, 12, 44, 01, 37000, TZStub(0,-30)))
        self.assertEquals(c('2001-03-02 12:44:01.000001+12:00'),
                  datetime(2001, 3, 2, 12, 44, 01, 1, TZStub(12,0)))
        self.assertEquals(c('2001-06-25 12:14:00-07'),
                  datetime(2001, 6, 25, 12, 14, 00, 0, TZStub(-7,0)))

    def test_conv_interval(self):
        from psycopg2da.adapter import _conv_interval
        from datetime import timedelta
        def c(s):
            return _conv_interval(s, None)

        self.assertEquals(c(''), None)
        self.assertEquals(c('01:00'), timedelta(hours=1))
        self.assertEquals(c('00:15'), timedelta(minutes=15))
        self.assertEquals(c('00:00:47'), timedelta(seconds=47))
        self.assertEquals(c('00:00:00.037'), timedelta(microseconds=37000))
        self.assertEquals(c('00:00:00.111111'), timedelta(microseconds=111111))
        self.assertEquals(c('1 day'), timedelta(days=1))
        self.assertEquals(c('2 days'), timedelta(days=2))
        self.assertEquals(c('374 days'), timedelta(days=374))
        self.assertEquals(c('2 days 03:20:15.123456'),
                          timedelta(days=2, hours=3, minutes=20,
                                    seconds=15, microseconds=123456))
        # XXX There's a problem with years and months.  Currently timedelta
        # cannot represent them accurately
        self.assertEquals(c('1 month'), '1 month')
        self.assertEquals(c('2 months'), '2 months')
        self.assertEquals(c('1 mon'), '1 mon')
        self.assertEquals(c('2 mons'), '2 mons')
        self.assertEquals(c('1 year'), '1 year')
        self.assertEquals(c('3 years'), '3 years')
        # Later we might be able to use
        ## self.assertEquals(c('1 month'), timedelta(months=1))
        ## self.assertEquals(c('2 months'), timedelta(months=2))
        ## self.assertEquals(c('1 year'), timedelta(years=1))
        ## self.assertEquals(c('3 years'), timedelta(years=3))

        self.assertRaises(ValueError, c, '2 day')
        self.assertRaises(ValueError, c, '2days')
        self.assertRaises(ValueError, c, '123')

    def test_conv_string(self):
        from psycopg2da.adapter import _get_string_conv
        _conv_string = _get_string_conv("utf-8")
        def c(s):
            return _conv_string(s, None)
        self.assertEquals(c(None), None)
        self.assertEquals(c(''), u'')
        self.assertEquals(c('c'), u'c')
        self.assertEquals(c('\xc3\x82\xc2\xa2'), u'\xc2\xa2')
        self.assertEquals(c('c\xc3\x82\xc2\xa2'), u'c\xc2\xa2')

class TestPsycopg2Adapter(TestCase):

    def setUp(self):
        import psycopg2da.adapter as adapter
        self.real_psycopg2 = adapter.psycopg2
        adapter.psycopg2 = self.psycopg2_stub = Psycopg2Stub()

    def tearDown(self):
        import psycopg2da.adapter as adapter
        adapter.psycopg2 = self.real_psycopg2

    def test_connection_factory(self):
        from psycopg2da.adapter import Psycopg2Adapter
        a = Psycopg2Adapter('dbi://username:password@hostname:port/dbname;junk=ignored')
        c = a._connection_factory()
        args = self.psycopg2_stub.last_connection_string.split()
        args.sort()
        self.assertEquals(args, ['dbname=dbname', 'host=hostname',
                                 'password=password', 'port=port',
                                 'user=username'])

    def test_registerTypes(self):
        import psycopg2da.adapter as adapter
        from psycopg2da.adapter import Psycopg2Adapter
        a = Psycopg2Adapter('dbi://')
        a.registerTypes()
        for typename in ('DATE', 'TIME', 'TIMETZ', 'TIMESTAMP',
                         'TIMESTAMPTZ', 'INTERVAL'):
            typeid = getattr(adapter, '%s_OID' % typename)
            result = self.psycopg2_stub.types.get(typeid, None)
            if not result:
                # comparing None with psycopg2.type object segfaults
                self.fail("did not register %s (%d): got None, not Z%s"
                          % (typename, typeid, typename))
            else:
                result_name = getattr(result, 'name', 'None')
                self.assertEquals(result, getattr(adapter, typename),
                              "did not register %s (%d): got %s, not Z%s"
                              % (typename, typeid, result_name, typename))


class TestISODateTime(TestCase):

    # Test if date/time parsing functions accept a sensible subset of ISO-8601
    # compliant date/time strings.
    #
    # Resources:
    #   http://www.cl.cam.ac.uk/~mgk25/iso-time.html
    #   http://www.mcs.vuw.ac.nz/technical/software/SGML/doc/iso8601/ISO8601.html
    #   http://www.w3.org/TR/NOTE-datetime
    #   http://www.ietf.org/rfc/rfc3339.txt

    basic_dates     = (('20020304',   (2002, 03, 04)),
                       ('20000229',   (2000, 02, 29)))

    extended_dates  = (('2002-03-04', (2002, 03, 04)),
                       ('2000-02-29', (2000, 02, 29)))

    basic_times     = (('12',         (12, 0, 0)),
                       ('1234',       (12, 34, 0)),
                       ('123417',     (12, 34, 17)),
                       ('123417.5',   (12, 34, 17.5)),
                       ('123417,5',   (12, 34, 17.5)))

    extended_times  = (('12',         (12, 0, 0)),
                       ('12:34',      (12, 34, 0)),
                       ('12:34:17',   (12, 34, 17)),
                       ('12:34:17.5', (12, 34, 17.5)),
                       ('12:34:17,5', (12, 34, 17.5)))

    basic_tzs       = (('Z', 0),
                       ('+02', 2*60),
                       ('+1130', 11*60+30),
                       ('-05', -5*60),
                       ('-0030', -30))

    extended_tzs    = (('Z', 0),
                       ('+02', 2*60),
                       ('+11:30', 11*60+30),
                       ('-05', -5*60),
                       ('-00:30', -30))

    time_separators = (' ', 'T')

    bad_dates       = ('', 'foo', 'XXXXXXXX', 'XXXX-XX-XX', '2001-2-29',
                       '1990/13/14')

    bad_times       = ('', 'foo', 'XXXXXX', '12:34,5', '12:34:56,')

    bad_timetzs     = ('12+12 34', '15:45 +1234', '18:00-12:34:56', '18:00+123', '18:00Q')

    bad_datetimes   = ('', 'foo', '2002-03-0412:33')

    bad_datetimetzs = ('', 'foo', '2002-03-04T12:33 +1200')

    exception_type  = ValueError

    # We need the following funcions:
    #   parse_date       -> (year, month, day)
    #   parse_time       -> (hour, minute, second)
    #   parse_timetz     -> (hour, minute, second, tzoffset)
    #   parse_datetime   -> (year, month, day, hour, minute, second)
    #   parse_datetimetz -> (year, month, day, hour, minute, second, tzoffset)
    # second can be a float, all other values are ints
    # tzoffset is offset in minutes east of UTC

    def setUp(self):
        from psycopg2da.adapter import parse_date, parse_time, \
                                parse_timetz, parse_datetime, parse_datetimetz
        self.parse_date = parse_date
        self.parse_time = parse_time
        self.parse_timetz = parse_timetz
        self.parse_datetime = parse_datetime
        self.parse_datetimetz = parse_datetimetz

    def test_basic_date(self):
        for s, d in self.basic_dates:
            self.assertEqual(self.parse_date(s), d)

    def test_extended_date(self):
        for s, d in self.extended_dates:
            self.assertEqual(self.parse_date(s), d)

    def test_bad_date(self):
        for s in self.bad_dates:
            self.assertRaises(self.exception_type, self.parse_date, s)

    def test_basic_time(self):
        for s, t in self.basic_times:
            self.assertEqual(self.parse_time(s), t)

    def test_extended_time(self):
        for s, t in self.extended_times:
            self.assertEqual(self.parse_time(s), t)

    def test_bad_time(self):
        for s in self.bad_times:
            self.assertRaises(self.exception_type, self.parse_time, s)

    def test_basic_timetz(self):
        for s, t in self.basic_times:
            for tz, off in self.basic_tzs:
                self.assertEqual(self.parse_timetz(s+tz), t + (off,))

    def test_extended_timetz(self):
        for s, t in self.extended_times:
            for tz, off in self.extended_tzs:
                self.assertEqual(self.parse_timetz(s+tz), t + (off,))

    def test_bad_timetzs(self):
        for s in self.bad_timetzs:
            self.assertRaises(self.exception_type, self.parse_timetz, s)

    def test_basic_datetime(self):
        for ds, d in self.basic_dates:
            for ts, t in self.basic_times:
                for sep in self.time_separators:
                    self.assertEqual(self.parse_datetime(ds+sep+ts), d + t)

    def test_extended_datetime(self):
        for ds, d in self.extended_dates:
            for ts, t in self.extended_times:
                for sep in self.time_separators:
                    self.assertEqual(self.parse_datetime(ds+sep+ts), d + t)

    def test_bad_datetimes(self):
        for s in self.bad_datetimes:
            self.assertRaises(self.exception_type, self.parse_datetime, s)

    def test_basic_datetimetz(self):
        for ds, d in self.basic_dates:
            for ts, t in self.basic_times:
                for tz, off in self.basic_tzs:
                    for sep in self.time_separators:
                        self.assertEqual(self.parse_datetimetz(ds+sep+ts+tz),
                                         d + t + (off,))

    def test_extended_datetimetz(self):
        for ds, d in self.extended_dates:
            for ts, t in self.extended_times:
                for tz, off in self.extended_tzs:
                    for sep in self.time_separators:
                        self.assertEqual(self.parse_datetimetz(ds+sep+ts+tz),
                                         d + t + (off,))

    def test_bad_datetimetzs(self):
        for s in self.bad_datetimetzs:
            self.assertRaises(self.exception_type, self.parse_datetimetz, s)


def test_suite():
    return TestSuite((
        makeSuite(TestPsycopg2TypeConversion),
        makeSuite(TestPsycopg2Adapter),
        makeSuite(TestISODateTime),
        ))

if __name__=='__main__':
    main(defaultTest='test_suite')
