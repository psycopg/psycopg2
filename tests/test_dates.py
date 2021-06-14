#!/usr/bin/env python

# test_dates.py - unit test for dates handling
#
# Copyright (C) 2008-2019 James Henstridge  <james@jamesh.id.au>
# Copyright (C) 2020-2021 The Psycopg Team
#
# psycopg2 is free software: you can redistribute it and/or modify it
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
# psycopg2 is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.

import sys
import math
import pickle
from datetime import date, datetime, time, timedelta, timezone

import psycopg2
from psycopg2.tz import FixedOffsetTimezone, ZERO
import unittest
from .testutils import ConnectingTestCase, skip_before_postgres, skip_if_crdb


def total_seconds(d):
    """Return total number of seconds of a timedelta as a float."""
    return d.days * 24 * 60 * 60 + d.seconds + d.microseconds / 1000000.0


class CommonDatetimeTestsMixin:

    def execute(self, *args):
        self.curs.execute(*args)
        return self.curs.fetchone()[0]

    def test_parse_date(self):
        value = self.DATE('2007-01-01', self.curs)
        self.assert_(value is not None)
        self.assertEqual(value.year, 2007)
        self.assertEqual(value.month, 1)
        self.assertEqual(value.day, 1)

    def test_parse_null_date(self):
        value = self.DATE(None, self.curs)
        self.assertEqual(value, None)

    def test_parse_incomplete_date(self):
        self.assertRaises(psycopg2.DataError, self.DATE, '2007', self.curs)
        self.assertRaises(psycopg2.DataError, self.DATE, '2007-01', self.curs)

    def test_parse_time(self):
        value = self.TIME('13:30:29', self.curs)
        self.assert_(value is not None)
        self.assertEqual(value.hour, 13)
        self.assertEqual(value.minute, 30)
        self.assertEqual(value.second, 29)

    def test_parse_null_time(self):
        value = self.TIME(None, self.curs)
        self.assertEqual(value, None)

    def test_parse_incomplete_time(self):
        self.assertRaises(psycopg2.DataError, self.TIME, '13', self.curs)
        self.assertRaises(psycopg2.DataError, self.TIME, '13:30', self.curs)

    def test_parse_datetime(self):
        value = self.DATETIME('2007-01-01 13:30:29', self.curs)
        self.assert_(value is not None)
        self.assertEqual(value.year, 2007)
        self.assertEqual(value.month, 1)
        self.assertEqual(value.day, 1)
        self.assertEqual(value.hour, 13)
        self.assertEqual(value.minute, 30)
        self.assertEqual(value.second, 29)

    def test_parse_null_datetime(self):
        value = self.DATETIME(None, self.curs)
        self.assertEqual(value, None)

    def test_parse_incomplete_datetime(self):
        self.assertRaises(psycopg2.DataError,
                          self.DATETIME, '2007', self.curs)
        self.assertRaises(psycopg2.DataError,
                          self.DATETIME, '2007-01', self.curs)
        self.assertRaises(psycopg2.DataError,
                          self.DATETIME, '2007-01-01 13', self.curs)
        self.assertRaises(psycopg2.DataError,
                          self.DATETIME, '2007-01-01 13:30', self.curs)

    def test_parse_null_interval(self):
        value = self.INTERVAL(None, self.curs)
        self.assertEqual(value, None)


class DatetimeTests(ConnectingTestCase, CommonDatetimeTestsMixin):
    """Tests for the datetime based date handling in psycopg2."""

    def setUp(self):
        ConnectingTestCase.setUp(self)
        self.curs = self.conn.cursor()
        self.DATE = psycopg2.extensions.PYDATE
        self.TIME = psycopg2.extensions.PYTIME
        self.DATETIME = psycopg2.extensions.PYDATETIME
        self.INTERVAL = psycopg2.extensions.PYINTERVAL

    def test_parse_bc_date(self):
        # datetime does not support BC dates
        self.assertRaises(ValueError, self.DATE, '00042-01-01 BC', self.curs)

    def test_parse_bc_datetime(self):
        # datetime does not support BC dates
        self.assertRaises(ValueError, self.DATETIME,
                          '00042-01-01 13:30:29 BC', self.curs)

    def test_parse_time_microseconds(self):
        value = self.TIME('13:30:29.123456', self.curs)
        self.assertEqual(value.second, 29)
        self.assertEqual(value.microsecond, 123456)

    def test_parse_datetime_microseconds(self):
        value = self.DATETIME('2007-01-01 13:30:29.123456', self.curs)
        self.assertEqual(value.second, 29)
        self.assertEqual(value.microsecond, 123456)

    def check_time_tz(self, str_offset, offset):
        base = time(13, 30, 29)
        base_str = '13:30:29'

        value = self.TIME(base_str + str_offset, self.curs)

        # Value has time zone info and correct UTC offset.
        self.assertNotEqual(value.tzinfo, None),
        self.assertEqual(value.utcoffset(), timedelta(seconds=offset))

        # Time portion is correct.
        self.assertEqual(value.replace(tzinfo=None), base)

    def test_parse_time_timezone(self):
        self.check_time_tz("+01", 3600)
        self.check_time_tz("-01", -3600)
        self.check_time_tz("+01:15", 4500)
        self.check_time_tz("-01:15", -4500)
        if sys.version_info < (3, 7):
            # The Python < 3.7 datetime module does not support time zone
            # offsets that are not a whole number of minutes.
            # We round the offset to the nearest minute.
            self.check_time_tz("+01:15:00", 60 * (60 + 15))
            self.check_time_tz("+01:15:29", 60 * (60 + 15))
            self.check_time_tz("+01:15:30", 60 * (60 + 16))
            self.check_time_tz("+01:15:59", 60 * (60 + 16))
            self.check_time_tz("-01:15:00", -60 * (60 + 15))
            self.check_time_tz("-01:15:29", -60 * (60 + 15))
            self.check_time_tz("-01:15:30", -60 * (60 + 16))
            self.check_time_tz("-01:15:59", -60 * (60 + 16))
        else:
            self.check_time_tz("+01:15:00", 60 * (60 + 15))
            self.check_time_tz("+01:15:29", 60 * (60 + 15) + 29)
            self.check_time_tz("+01:15:30", 60 * (60 + 15) + 30)
            self.check_time_tz("+01:15:59", 60 * (60 + 15) + 59)
            self.check_time_tz("-01:15:00", -(60 * (60 + 15)))
            self.check_time_tz("-01:15:29", -(60 * (60 + 15) + 29))
            self.check_time_tz("-01:15:30", -(60 * (60 + 15) + 30))
            self.check_time_tz("-01:15:59", -(60 * (60 + 15) + 59))

    def check_datetime_tz(self, str_offset, offset):
        base = datetime(2007, 1, 1, 13, 30, 29)
        base_str = '2007-01-01 13:30:29'

        value = self.DATETIME(base_str + str_offset, self.curs)

        # Value has time zone info and correct UTC offset.
        self.assertNotEqual(value.tzinfo, None),
        self.assertEqual(value.utcoffset(), timedelta(seconds=offset))

        # Datetime is correct.
        self.assertEqual(value.replace(tzinfo=None), base)

        # Conversion to UTC produces the expected offset.
        UTC = timezone(timedelta(0))
        value_utc = value.astimezone(UTC).replace(tzinfo=None)
        self.assertEqual(base - value_utc, timedelta(seconds=offset))

    def test_default_tzinfo(self):
        self.curs.execute("select '2000-01-01 00:00+02:00'::timestamptz")
        dt = self.curs.fetchone()[0]
        self.assert_(isinstance(dt.tzinfo, timezone))
        self.assertEqual(dt,
            datetime(2000, 1, 1, tzinfo=timezone(timedelta(minutes=120))))

    def test_fotz_tzinfo(self):
        self.curs.tzinfo_factory = FixedOffsetTimezone
        self.curs.execute("select '2000-01-01 00:00+02:00'::timestamptz")
        dt = self.curs.fetchone()[0]
        self.assert_(not isinstance(dt.tzinfo, timezone))
        self.assert_(isinstance(dt.tzinfo, FixedOffsetTimezone))
        self.assertEqual(dt,
            datetime(2000, 1, 1, tzinfo=timezone(timedelta(minutes=120))))

    def test_parse_datetime_timezone(self):
        self.check_datetime_tz("+01", 3600)
        self.check_datetime_tz("-01", -3600)
        self.check_datetime_tz("+01:15", 4500)
        self.check_datetime_tz("-01:15", -4500)
        if sys.version_info < (3, 7):
            # The Python < 3.7 datetime module does not support time zone
            # offsets that are not a whole number of minutes.
            # We round the offset to the nearest minute.
            self.check_datetime_tz("+01:15:00", 60 * (60 + 15))
            self.check_datetime_tz("+01:15:29", 60 * (60 + 15))
            self.check_datetime_tz("+01:15:30", 60 * (60 + 16))
            self.check_datetime_tz("+01:15:59", 60 * (60 + 16))
            self.check_datetime_tz("-01:15:00", -60 * (60 + 15))
            self.check_datetime_tz("-01:15:29", -60 * (60 + 15))
            self.check_datetime_tz("-01:15:30", -60 * (60 + 16))
            self.check_datetime_tz("-01:15:59", -60 * (60 + 16))
        else:
            self.check_datetime_tz("+01:15:00", 60 * (60 + 15))
            self.check_datetime_tz("+01:15:29", 60 * (60 + 15) + 29)
            self.check_datetime_tz("+01:15:30", 60 * (60 + 15) + 30)
            self.check_datetime_tz("+01:15:59", 60 * (60 + 15) + 59)
            self.check_datetime_tz("-01:15:00", -(60 * (60 + 15)))
            self.check_datetime_tz("-01:15:29", -(60 * (60 + 15) + 29))
            self.check_datetime_tz("-01:15:30", -(60 * (60 + 15) + 30))
            self.check_datetime_tz("-01:15:59", -(60 * (60 + 15) + 59))

    def test_parse_time_no_timezone(self):
        self.assertEqual(self.TIME("13:30:29", self.curs).tzinfo, None)
        self.assertEqual(self.TIME("13:30:29.123456", self.curs).tzinfo, None)

    def test_parse_datetime_no_timezone(self):
        self.assertEqual(
            self.DATETIME("2007-01-01 13:30:29", self.curs).tzinfo, None)
        self.assertEqual(
            self.DATETIME("2007-01-01 13:30:29.123456", self.curs).tzinfo, None)

    def test_parse_interval(self):
        value = self.INTERVAL('42 days 12:34:56.123456', self.curs)
        self.assertNotEqual(value, None)
        self.assertEqual(value.days, 42)
        self.assertEqual(value.seconds, 45296)
        self.assertEqual(value.microseconds, 123456)

    def test_parse_negative_interval(self):
        value = self.INTERVAL('-42 days -12:34:56.123456', self.curs)
        self.assertNotEqual(value, None)
        self.assertEqual(value.days, -43)
        self.assertEqual(value.seconds, 41103)
        self.assertEqual(value.microseconds, 876544)

    def test_parse_infinity(self):
        value = self.DATETIME('-infinity', self.curs)
        self.assertEqual(str(value), '0001-01-01 00:00:00')
        value = self.DATETIME('infinity', self.curs)
        self.assertEqual(str(value), '9999-12-31 23:59:59.999999')
        value = self.DATE('infinity', self.curs)
        self.assertEqual(str(value), '9999-12-31')

    def test_adapt_date(self):
        value = self.execute('select (%s)::date::text',
                             [date(2007, 1, 1)])
        self.assertEqual(value, '2007-01-01')

    def test_adapt_time(self):
        value = self.execute('select (%s)::time::text',
                             [time(13, 30, 29)])
        self.assertEqual(value, '13:30:29')

    @skip_if_crdb("cast adds tz")
    def test_adapt_datetime(self):
        value = self.execute('select (%s)::timestamp::text',
                             [datetime(2007, 1, 1, 13, 30, 29)])
        self.assertEqual(value, '2007-01-01 13:30:29')

    def test_adapt_timedelta(self):
        value = self.execute('select extract(epoch from (%s)::interval)',
                             [timedelta(days=42, seconds=45296,
                                        microseconds=123456)])
        seconds = math.floor(value)
        self.assertEqual(seconds, 3674096)
        self.assertEqual(int(round((value - seconds) * 1000000)), 123456)

    def test_adapt_negative_timedelta(self):
        value = self.execute('select extract(epoch from (%s)::interval)',
                             [timedelta(days=-42, seconds=45296,
                                        microseconds=123456)])
        seconds = math.floor(value)
        self.assertEqual(seconds, -3583504)
        self.assertEqual(int(round((value - seconds) * 1000000)), 123456)

    def _test_type_roundtrip(self, o1):
        o2 = self.execute("select %s;", (o1,))
        self.assertEqual(type(o1), type(o2))
        return o2

    def _test_type_roundtrip_array(self, o1):
        o1 = [o1]
        o2 = self.execute("select %s;", (o1,))
        self.assertEqual(type(o1[0]), type(o2[0]))

    def test_type_roundtrip_date(self):
        self._test_type_roundtrip(date(2010, 5, 3))

    def test_type_roundtrip_datetime(self):
        dt = self._test_type_roundtrip(datetime(2010, 5, 3, 10, 20, 30))
        self.assertEqual(None, dt.tzinfo)

    def test_type_roundtrip_datetimetz(self):
        tz = timezone(timedelta(minutes=8 * 60))
        dt1 = datetime(2010, 5, 3, 10, 20, 30, tzinfo=tz)
        dt2 = self._test_type_roundtrip(dt1)
        self.assertNotEqual(None, dt2.tzinfo)
        self.assertEqual(dt1, dt2)

    def test_type_roundtrip_time(self):
        tm = self._test_type_roundtrip(time(10, 20, 30))
        self.assertEqual(None, tm.tzinfo)

    def test_type_roundtrip_timetz(self):
        tz = timezone(timedelta(minutes=8 * 60))
        tm1 = time(10, 20, 30, tzinfo=tz)
        tm2 = self._test_type_roundtrip(tm1)
        self.assertNotEqual(None, tm2.tzinfo)
        self.assertEqual(tm1, tm2)

    def test_type_roundtrip_interval(self):
        self._test_type_roundtrip(timedelta(seconds=30))

    def test_type_roundtrip_date_array(self):
        self._test_type_roundtrip_array(date(2010, 5, 3))

    def test_type_roundtrip_datetime_array(self):
        self._test_type_roundtrip_array(datetime(2010, 5, 3, 10, 20, 30))

    def test_type_roundtrip_datetimetz_array(self):
        self._test_type_roundtrip_array(
            datetime(2010, 5, 3, 10, 20, 30, tzinfo=timezone(timedelta(0))))

    def test_type_roundtrip_time_array(self):
        self._test_type_roundtrip_array(time(10, 20, 30))

    def test_type_roundtrip_interval_array(self):
        self._test_type_roundtrip_array(timedelta(seconds=30))

    @skip_before_postgres(8, 1)
    def test_time_24(self):
        t = self.execute("select '24:00'::time;")
        self.assertEqual(t, time(0, 0))

        t = self.execute("select '24:00+05'::timetz;")
        self.assertEqual(t, time(0, 0, tzinfo=timezone(timedelta(minutes=300))))

        t = self.execute("select '24:00+05:30'::timetz;")
        self.assertEqual(t, time(0, 0, tzinfo=timezone(timedelta(minutes=330))))

    @skip_before_postgres(8, 1)
    def test_large_interval(self):
        t = self.execute("select '999999:00:00'::interval")
        self.assertEqual(total_seconds(t), 999999 * 60 * 60)

        t = self.execute("select '-999999:00:00'::interval")
        self.assertEqual(total_seconds(t), -999999 * 60 * 60)

        t = self.execute("select '999999:00:00.1'::interval")
        self.assertEqual(total_seconds(t), 999999 * 60 * 60 + 0.1)

        t = self.execute("select '999999:00:00.9'::interval")
        self.assertEqual(total_seconds(t), 999999 * 60 * 60 + 0.9)

        t = self.execute("select '-999999:00:00.1'::interval")
        self.assertEqual(total_seconds(t), -999999 * 60 * 60 - 0.1)

        t = self.execute("select '-999999:00:00.9'::interval")
        self.assertEqual(total_seconds(t), -999999 * 60 * 60 - 0.9)

    def test_micros_rounding(self):
        t = self.execute("select '0.1'::interval")
        self.assertEqual(total_seconds(t), 0.1)

        t = self.execute("select '0.01'::interval")
        self.assertEqual(total_seconds(t), 0.01)

        t = self.execute("select '0.000001'::interval")
        self.assertEqual(total_seconds(t), 1e-6)

        t = self.execute("select '0.0000004'::interval")
        self.assertEqual(total_seconds(t), 0)

        t = self.execute("select '0.0000006'::interval")
        self.assertEqual(total_seconds(t), 1e-6)

    def test_interval_overflow(self):
        cur = self.conn.cursor()
        # hack a cursor to receive values too extreme to be represented
        # but still I want an error, not a random number
        psycopg2.extensions.register_type(
            psycopg2.extensions.new_type(
                psycopg2.STRING.values, 'WAT', psycopg2.extensions.INTERVAL),
            cur)

        def f(val):
            cur.execute(f"select '{val}'::text")
            return cur.fetchone()[0]

        self.assertRaises(OverflowError, f, '100000000000000000:00:00')
        self.assertRaises(OverflowError, f, '00:100000000000000000:00:00')
        self.assertRaises(OverflowError, f, '00:00:100000000000000000:00')
        self.assertRaises(OverflowError, f, '00:00:00.100000000000000000')

    @skip_if_crdb("infinity date")
    def test_adapt_infinity_tz(self):
        t = self.execute("select 'infinity'::timestamp")
        self.assert_(t.tzinfo is None)
        self.assert_(t > datetime(4000, 1, 1))

        t = self.execute("select '-infinity'::timestamp")
        self.assert_(t.tzinfo is None)
        self.assert_(t < datetime(1000, 1, 1))

        t = self.execute("select 'infinity'::timestamptz")
        self.assert_(t.tzinfo is not None)
        self.assert_(t > datetime(4000, 1, 1, tzinfo=timezone(timedelta(0))))

        t = self.execute("select '-infinity'::timestamptz")
        self.assert_(t.tzinfo is not None)
        self.assert_(t < datetime(1000, 1, 1, tzinfo=timezone(timedelta(0))))

    def test_redshift_day(self):
        # Redshift is reported returning 1 day interval as microsec (bug #558)
        cur = self.conn.cursor()
        psycopg2.extensions.register_type(
            psycopg2.extensions.new_type(
                psycopg2.STRING.values, 'WAT', psycopg2.extensions.INTERVAL),
            cur)

        for s, v in [
            ('0', timedelta(0)),
            ('1', timedelta(microseconds=1)),
            ('-1', timedelta(microseconds=-1)),
            ('1000000', timedelta(seconds=1)),
            ('86400000000', timedelta(days=1)),
            ('-86400000000', timedelta(days=-1)),
        ]:
            cur.execute("select %s::text", (s,))
            r = cur.fetchone()[0]
            self.assertEqual(r, v, f"{s} -> {r} != {v}")

    @skip_if_crdb("interval style")
    @skip_before_postgres(8, 4)
    def test_interval_iso_8601_not_supported(self):
        # We may end up supporting, but no pressure for it
        cur = self.conn.cursor()
        cur.execute("set local intervalstyle to iso_8601")
        cur.execute("select '1 day 2 hours'::interval")
        self.assertRaises(psycopg2.NotSupportedError, cur.fetchone)


class FromTicksTestCase(unittest.TestCase):
    # bug "TimestampFromTicks() throws ValueError (2-2.0.14)"
    # reported by Jozsef Szalay on 2010-05-06
    def test_timestamp_value_error_sec_59_99(self):
        s = psycopg2.TimestampFromTicks(1273173119.99992)
        self.assertEqual(s.adapted,
            datetime(2010, 5, 6, 14, 11, 59, 999920,
                tzinfo=timezone(timedelta(minutes=-5 * 60))))

    def test_date_value_error_sec_59_99(self):
        s = psycopg2.DateFromTicks(1273173119.99992)
        # The returned date is local
        self.assert_(s.adapted in [date(2010, 5, 6), date(2010, 5, 7)])

    def test_time_value_error_sec_59_99(self):
        s = psycopg2.TimeFromTicks(1273173119.99992)
        self.assertEqual(s.adapted.replace(hour=0),
            time(0, 11, 59, 999920))


class FixedOffsetTimezoneTests(unittest.TestCase):

    def test_init_with_no_args(self):
        tzinfo = FixedOffsetTimezone()
        self.assert_(tzinfo._offset is ZERO)
        self.assert_(tzinfo._name is None)

    def test_repr_with_positive_offset(self):
        tzinfo = FixedOffsetTimezone(5 * 60)
        self.assertEqual(repr(tzinfo),
            "psycopg2.tz.FixedOffsetTimezone(offset=%r, name=None)"
            % timedelta(minutes=5 * 60))

    def test_repr_with_negative_offset(self):
        tzinfo = FixedOffsetTimezone(-5 * 60)
        self.assertEqual(repr(tzinfo),
            "psycopg2.tz.FixedOffsetTimezone(offset=%r, name=None)"
            % timedelta(minutes=-5 * 60))

    def test_init_with_timedelta(self):
        td = timedelta(minutes=5 * 60)
        tzinfo = FixedOffsetTimezone(td)
        self.assertEqual(tzinfo, FixedOffsetTimezone(5 * 60))
        self.assertEqual(repr(tzinfo),
            "psycopg2.tz.FixedOffsetTimezone(offset=%r, name=None)" % td)

    def test_repr_with_name(self):
        tzinfo = FixedOffsetTimezone(name="FOO")
        self.assertEqual(repr(tzinfo),
            "psycopg2.tz.FixedOffsetTimezone(offset=%r, name='FOO')"
            % timedelta(0))

    def test_instance_caching(self):
        self.assert_(FixedOffsetTimezone(name="FOO")
            is FixedOffsetTimezone(name="FOO"))
        self.assert_(FixedOffsetTimezone(7 * 60)
            is FixedOffsetTimezone(7 * 60))
        self.assert_(FixedOffsetTimezone(-9 * 60, 'FOO')
            is FixedOffsetTimezone(-9 * 60, 'FOO'))
        self.assert_(FixedOffsetTimezone(9 * 60)
            is not FixedOffsetTimezone(9 * 60, 'FOO'))
        self.assert_(FixedOffsetTimezone(name='FOO')
            is not FixedOffsetTimezone(9 * 60, 'FOO'))

    def test_pickle(self):
        # ticket #135
        tz11 = FixedOffsetTimezone(60)
        tz12 = FixedOffsetTimezone(120)
        for proto in [-1, 0, 1, 2]:
            tz21, tz22 = pickle.loads(pickle.dumps([tz11, tz12], proto))
            self.assertEqual(tz11, tz21)
            self.assertEqual(tz12, tz22)

        tz11 = FixedOffsetTimezone(60, name='foo')
        tz12 = FixedOffsetTimezone(120, name='bar')
        for proto in [-1, 0, 1, 2]:
            tz21, tz22 = pickle.loads(pickle.dumps([tz11, tz12], proto))
            self.assertEqual(tz11, tz21)
            self.assertEqual(tz12, tz22)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)


if __name__ == "__main__":
    unittest.main()
