"""Error classes for PostgreSQL error codes
"""

from psycopg2._psycopg import (  # noqa
    Error, Warning, DataError, DatabaseError, ProgrammingError, IntegrityError,
    InterfaceError, InternalError, NotSupportedError, OperationalError,
    QueryCanceledError, TransactionRollbackError)


_by_sqlstate = {}


class UndefinedTable(ProgrammingError):
    pass

_by_sqlstate['42P01'] = UndefinedTable
