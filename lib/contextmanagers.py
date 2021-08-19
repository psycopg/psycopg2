"""psycopg context managers for connection and cursor objects."""


from contextlib import contextmanager
from extensions import connect, cursor


@contextmanager
def connection(**kwargs):
    """Context manager to yield and close a db connection."""
    connection = connect(**kwargs)

    try:
        yield connection

    finally:
        connection.close()


@contextmanager
def cursor(connection):
    """
    Context manager to yield and close a cursor given an existing db
    connection.
    """
    cursor = connection.cursor()

    try:
        yield cursor

    finally:
        cursor.close()


@contextmanager
def connection_cursor(**kwargs):
    """
    Context manager to yield a cursor and close it and its connection for
    purposes or performing a single db call.
    """
    connection = connect(**kwargs)
    cursor = connection.cursor()

    try:
        yield cursor

    finally:
        cursor.close()
        connection.close()
