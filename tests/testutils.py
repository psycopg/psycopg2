# testutils.py - utility module for psycopg2 testing.

#
# Copyright (C) 2010-2019 Daniele Varrazzo  <daniele.varrazzo@gmail.com>
# Copyright (C) 2020 The Psycopg Team
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


import re
import os
import sys
import types
import ctypes
import select
import operator
import platform
import unittest
from functools import wraps
from ctypes.util import find_library

import psycopg2
import psycopg2.errors
import psycopg2.extensions
from psycopg2.compat import PY2, PY3, string_types, text_type

from .testconfig import green, dsn, repl_dsn

# Python 2/3 compatibility

if PY2:
    # Python 2
    from StringIO import StringIO
    TextIOBase = object
    long = long
    reload = reload
    unichr = unichr

else:
    # Python 3
    from io import StringIO         # noqa
    from io import TextIOBase       # noqa
    from importlib import reload    # noqa
    long = int
    unichr = chr


# Silence warnings caused by the stubbornness of the Python unittest
# maintainers
# https://bugs.python.org/issue9424
if (not hasattr(unittest.TestCase, 'assert_')
        or unittest.TestCase.assert_ is not unittest.TestCase.assertTrue):
    # mavaff...
    unittest.TestCase.assert_ = unittest.TestCase.assertTrue
    unittest.TestCase.failUnless = unittest.TestCase.assertTrue
    unittest.TestCase.assertEquals = unittest.TestCase.assertEqual
    unittest.TestCase.failUnlessEqual = unittest.TestCase.assertEqual


def assertDsnEqual(self, dsn1, dsn2, msg=None):
    """Check that two conninfo string have the same content"""
    self.assertEqual(set(dsn1.split()), set(dsn2.split()), msg)


unittest.TestCase.assertDsnEqual = assertDsnEqual


class ConnectingTestCase(unittest.TestCase):
    """A test case providing connections for tests.

    A connection for the test is always available as `self.conn`. Others can be
    created with `self.connect()`. All are closed on tearDown.

    Subclasses needing to customize setUp and tearDown should remember to call
    the base class implementations.
    """
    def setUp(self):
        self._conns = []

    def tearDown(self):
        # close the connections used in the test
        for conn in self._conns:
            if not conn.closed:
                conn.close()

    def assertQuotedEqual(self, first, second, msg=None):
        """Compare two quoted strings disregarding eventual E'' quotes"""
        def f(s):
            if isinstance(s, text_type):
                return re.sub(r"\bE'", "'", s)
            elif isinstance(first, bytes):
                return re.sub(br"\bE'", b"'", s)
            else:
                return s

        return self.assertEqual(f(first), f(second), msg)

    def connect(self, **kwargs):
        try:
            self._conns
        except AttributeError as e:
            raise AttributeError(
                "%s (did you forget to call ConnectingTestCase.setUp()?)"
                % e)

        if 'dsn' in kwargs:
            conninfo = kwargs.pop('dsn')
        else:
            conninfo = dsn
        conn = psycopg2.connect(conninfo, **kwargs)
        self._conns.append(conn)
        return conn

    def repl_connect(self, **kwargs):
        """Return a connection set up for replication

        The connection is on "PSYCOPG2_TEST_REPL_DSN" unless overridden by
        a *dsn* kwarg.

        Should raise a skip test if not available, but guard for None on
        old Python versions.
        """
        if repl_dsn is None:
            return self.skipTest("replication tests disabled by default")

        if 'dsn' not in kwargs:
            kwargs['dsn'] = repl_dsn
        try:
            conn = self.connect(**kwargs)
            if conn.async_ == 1:
                self.wait(conn)
        except psycopg2.OperationalError as e:
            # If pgcode is not set it is a genuine connection error
            # Otherwise we tried to run some bad operation in the connection
            # (e.g. bug #482) and we'd rather know that.
            if e.pgcode is None:
                return self.skipTest("replication db not configured: %s" % e)
            else:
                raise

        return conn

    def _get_conn(self):
        if not hasattr(self, '_the_conn'):
            self._the_conn = self.connect()

        return self._the_conn

    def _set_conn(self, conn):
        self._the_conn = conn

    conn = property(_get_conn, _set_conn)

    # for use with async connections only
    def wait(self, cur_or_conn):
        pollable = cur_or_conn
        if not hasattr(pollable, 'poll'):
            pollable = cur_or_conn.connection
        while True:
            state = pollable.poll()
            if state == psycopg2.extensions.POLL_OK:
                break
            elif state == psycopg2.extensions.POLL_READ:
                select.select([pollable], [], [], 1)
            elif state == psycopg2.extensions.POLL_WRITE:
                select.select([], [pollable], [], 1)
            else:
                raise Exception("Unexpected result from poll: %r", state)

    _libpq = None

    @property
    def libpq(self):
        """Return a ctypes wrapper for the libpq library"""
        if ConnectingTestCase._libpq is not None:
            return ConnectingTestCase._libpq

        libname = find_library('pq')
        if libname is None and platform.system() == 'Windows':
            raise self.skipTest("can't import libpq on windows")

        rv = ConnectingTestCase._libpq = ctypes.pydll.LoadLibrary(libname)
        return rv


def decorate_all_tests(obj, *decorators):
    """
    Apply all the *decorators* to all the tests defined in the TestCase *obj*.

    The decorator can also be applied to a decorator: if *obj* is a function,
    return a new decorator which can be applied either to a method or to a
    class, in which case it will decorate all the tests.
    """
    if isinstance(obj, types.FunctionType):
        def decorator(func_or_cls):
            if isinstance(func_or_cls, types.FunctionType):
                return obj(func_or_cls)
            else:
                decorate_all_tests(func_or_cls, obj)
                return func_or_cls

        return decorator

    for n in dir(obj):
        if n.startswith('test'):
            for d in decorators:
                setattr(obj, n, d(getattr(obj, n)))


@decorate_all_tests
def skip_if_no_uuid(f):
    """Decorator to skip a test if uuid is not supported by PG."""
    @wraps(f)
    def skip_if_no_uuid_(self):
        try:
            cur = self.conn.cursor()
            cur.execute("select typname from pg_type where typname = 'uuid'")
            has = cur.fetchone()
        finally:
            self.conn.rollback()

        if has:
            return f(self)
        else:
            return self.skipTest("uuid type not available on the server")

    return skip_if_no_uuid_


@decorate_all_tests
def skip_if_tpc_disabled(f):
    """Skip a test if the server has tpc support disabled."""
    @wraps(f)
    def skip_if_tpc_disabled_(self):
        cnn = self.connect()
        skip_if_crdb("2-phase commit", cnn)

        cur = cnn.cursor()
        try:
            cur.execute("SHOW max_prepared_transactions;")
        except psycopg2.ProgrammingError:
            return self.skipTest(
                "server too old: two phase transactions not supported.")
        else:
            mtp = int(cur.fetchone()[0])
        cnn.close()

        if not mtp:
            return self.skipTest(
                "server not configured for two phase transactions. "
                "set max_prepared_transactions to > 0 to run the test")
        return f(self)

    return skip_if_tpc_disabled_


def skip_before_postgres(*ver):
    """Skip a test on PostgreSQL before a certain version."""
    reason = None
    if isinstance(ver[-1], str):
        ver, reason = ver[:-1], ver[-1]

    ver = ver + (0,) * (3 - len(ver))

    @decorate_all_tests
    def skip_before_postgres_(f):
        @wraps(f)
        def skip_before_postgres__(self):
            if self.conn.info.server_version < int("%d%02d%02d" % ver):
                return self.skipTest(
                    reason or "skipped because PostgreSQL %s"
                    % self.conn.info.server_version)
            else:
                return f(self)

        return skip_before_postgres__
    return skip_before_postgres_


def skip_after_postgres(*ver):
    """Skip a test on PostgreSQL after (including) a certain version."""
    ver = ver + (0,) * (3 - len(ver))

    @decorate_all_tests
    def skip_after_postgres_(f):
        @wraps(f)
        def skip_after_postgres__(self):
            if self.conn.info.server_version >= int("%d%02d%02d" % ver):
                return self.skipTest("skipped because PostgreSQL %s"
                    % self.conn.info.server_version)
            else:
                return f(self)

        return skip_after_postgres__
    return skip_after_postgres_


def libpq_version():
    v = psycopg2.__libpq_version__
    if v >= 90100:
        v = min(v, psycopg2.extensions.libpq_version())
    return v


def skip_before_libpq(*ver):
    """Skip a test if libpq we're linked to is older than a certain version."""
    ver = ver + (0,) * (3 - len(ver))

    def skip_before_libpq_(cls):
        v = libpq_version()
        decorator = unittest.skipIf(
            v < int("%d%02d%02d" % ver),
            "skipped because libpq %d" % v,
        )
        return decorator(cls)
    return skip_before_libpq_


def skip_after_libpq(*ver):
    """Skip a test if libpq we're linked to is newer than a certain version."""
    ver = ver + (0,) * (3 - len(ver))

    def skip_after_libpq_(cls):
        v = libpq_version()
        decorator = unittest.skipIf(
            v >= int("%d%02d%02d" % ver),
            "skipped because libpq %s" % v,
        )
        return decorator(cls)
    return skip_after_libpq_


def skip_before_python(*ver):
    """Skip a test on Python before a certain version."""
    def skip_before_python_(cls):
        decorator = unittest.skipIf(
            sys.version_info[:len(ver)] < ver,
            "skipped because Python %s"
            % ".".join(map(str, sys.version_info[:len(ver)])),
        )
        return decorator(cls)
    return skip_before_python_


def skip_from_python(*ver):
    """Skip a test on Python after (including) a certain version."""
    def skip_from_python_(cls):
        decorator = unittest.skipIf(
            sys.version_info[:len(ver)] >= ver,
            "skipped because Python %s"
            % ".".join(map(str, sys.version_info[:len(ver)])),
        )
        return decorator(cls)
    return skip_from_python_


@decorate_all_tests
def skip_if_no_superuser(f):
    """Skip a test if the database user running the test is not a superuser"""
    @wraps(f)
    def skip_if_no_superuser_(self):
        try:
            return f(self)
        except psycopg2.errors.InsufficientPrivilege:
            self.skipTest("skipped because not superuser")

    return skip_if_no_superuser_


def skip_if_green(reason):
    def skip_if_green_(cls):
        decorator = unittest.skipIf(green, reason)
        return decorator(cls)
    return skip_if_green_


skip_copy_if_green = skip_if_green("copy in async mode currently not supported")


def skip_if_no_getrefcount(cls):
    decorator = unittest.skipUnless(
        hasattr(sys, 'getrefcount'),
        'no sys.getrefcount()',
    )
    return decorator(cls)


def skip_if_windows(cls):
    """Skip a test if run on windows"""
    decorator = unittest.skipIf(
        platform.system() == 'Windows',
        "Not supported on Windows",
    )
    return decorator(cls)


def crdb_version(conn, __crdb_version=[]):
    """
    Return the CockroachDB version if that's the db being tested, else None.

    Return the number as an integer similar to PQserverVersion: return
    v20.1.3 as 200103.

    Assume all the connections are on the same db: return a cached result on
    following calls.

    """
    if __crdb_version:
        return __crdb_version[0]

    sver = conn.info.parameter_status("crdb_version")
    if sver is None:
        __crdb_version.append(None)
    else:
        m = re.search(r"\bv(\d+)\.(\d+)\.(\d+)", sver)
        if not m:
            raise ValueError(
                "can't parse CockroachDB version from %s" % sver)

        ver = int(m.group(1)) * 10000 + int(m.group(2)) * 100 + int(m.group(3))
        __crdb_version.append(ver)

    return __crdb_version[0]


def skip_if_crdb(reason, conn=None, version=None):
    """Skip a test or test class if we are testing against CockroachDB.

    Can be used as a decorator for tests function or classes:

        @skip_if_crdb("my reason")
        class SomeUnitTest(UnitTest):
            # ...

    Or as a normal function if the *conn* argument is passed.

    If *version* is specified it should be a string such as ">= 20.1", "< 20",
    "== 20.1.3": the test will be skipped only if the version matches.

    """
    if not isinstance(reason, string_types):
        raise TypeError("reason should be a string, got %r instead" % reason)

    if conn is not None:
        ver = crdb_version(conn)
        if ver is not None and _crdb_match_version(ver, version):
            if reason in crdb_reasons:
                reason = (
                    "%s (https://github.com/cockroachdb/cockroach/issues/%s)"
                    % (reason, crdb_reasons[reason]))
            raise unittest.SkipTest(
                "not supported on CockroachDB %s: %s" % (ver, reason))

    @decorate_all_tests
    def skip_if_crdb_(f):
        @wraps(f)
        def skip_if_crdb__(self, *args, **kwargs):
            skip_if_crdb(reason, conn=self.connect(), version=version)
            return f(self, *args, **kwargs)

        return skip_if_crdb__

    return skip_if_crdb_


# mapping from reason description to ticket number
crdb_reasons = {
    "2-phase commit": 22329,
    "backend pid": 35897,
    "cancel": 41335,
    "cast adds tz": 51692,
    "cidr": 18846,
    "composite": 27792,
    "copy": 41608,
    "deferrable": 48307,
    "encoding": 35882,
    "hstore": 41284,
    "infinity date": 41564,
    "interval style": 35807,
    "large objects": 243,
    "named cursor": 41412,
    "nested array": 32552,
    "notify": 41522,
    "range": 41282,
    "stored procedure": 1751,
}


def _crdb_match_version(version, pattern):
    if pattern is None:
        return True

    m = re.match(r'^(>|>=|<|<=|==|!=)\s*(\d+)(?:\.(\d+))?(?:\.(\d+))?$', pattern)
    if m is None:
        raise ValueError(
            "bad crdb version pattern %r: should be 'OP MAJOR[.MINOR[.BUGFIX]]'"
            % pattern)

    ops = {'>': 'gt', '>=': 'ge', '<': 'lt', '<=': 'le', '==': 'eq', '!=': 'ne'}
    op = getattr(operator, ops[m.group(1)])
    ref = int(m.group(2)) * 10000 + int(m.group(3) or 0) * 100 + int(m.group(4) or 0)
    return op(version, ref)


class py3_raises_typeerror(object):
    def __enter__(self):
        pass

    def __exit__(self, type, exc, tb):
        if PY3:
            assert type is TypeError
            return True


def slow(f):
    """Decorator to mark slow tests we may want to skip

    Note: in order to find slow tests you can run:

    make check 2>&1 | ts -i "%.s" | sort -n
    """
    @wraps(f)
    def slow_(self):
        if os.environ.get('PSYCOPG2_TEST_FAST', '0') != '0':
            return self.skipTest("slow test")
        return f(self)
    return slow_


def restore_types(f):
    """Decorator to restore the adaptation system after running a test"""
    @wraps(f)
    def restore_types_(self):
        types = psycopg2.extensions.string_types.copy()
        adapters = psycopg2.extensions.adapters.copy()
        try:
            return f(self)
        finally:
            psycopg2.extensions.string_types.clear()
            psycopg2.extensions.string_types.update(types)
            psycopg2.extensions.adapters.clear()
            psycopg2.extensions.adapters.update(adapters)

    return restore_types_
