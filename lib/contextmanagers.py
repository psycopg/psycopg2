"""psycopg context managers for connection and cursor objects."""


import psycopg2
from contextlib import contextmanager


@contextmanager
def connect(**kwargs):
    """Context manager to yield and close a db connection."""
    conn = psycopg2.connect(**kwargs)

    try:
        yield conn

    finally:
        conn.close()


@contextmanager
def cursor(conn):
    """
    Context manager to yield and close a cursor given an existing db
    connection.
    """
    cursor = conn.cursor()

    try:
        yield cursor

    finally:
        cursor.close()


@contextmanager
def connect_cursor(**kwargs):
    """
    Context manager to yield a cursor and close it and its connection for
    purposes or performing a single db call.
    """
    conn = psycopg2.connect(**kwargs)
    cur = conn.cursor()

    try:
        yield cur

    finally:
        cur.close()
        conn.close()
