=================================================
Psycopg -- PostgreSQL database adapter for Python
=================================================

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

Psycopg is a PostgreSQL_ database adapter for the Python_ programming
language.  Its main advantages are that it supports the full Python |DBAPI|_
and it is thread safe (threads can share the connections). It was designed for
heavily multi-threaded applications that create and destroy lots of cursors and
make a conspicuous number of concurrent :sql:`INSERT`\ s or :sql:`UPDATE`\ s.
The psycopg distribution includes ZPsycopgDA, a Zope_ Database Adapter.

Psycopg 2 is an almost complete rewrite of the Psycopg 1.1.x branch. Psycopg 2
features complete libpq_ v3 protocol, |COPY-TO-FROM|__ and full :ref:`object
adaptation <python-types-adaptation>` for all basic Python types: strings (including unicode), ints,
longs, floats, buffers (binary objects), booleans, `mx.DateTime`_ and builtin
datetime types. It also supports unicode queries and Python lists mapped to
PostgreSQL arrays.

.. _PostgreSQL: http://www.postgresql.org/
.. _Python: http://www.python.org/
.. _Zope: http://www.zope.org/
.. _libpq: http://www.postgresql.org/docs/9.0/static/libpq.html
.. |COPY-TO-FROM| replace:: :sql:`COPY TO/COPY FROM`
.. __: http://www.postgresql.org/docs/9.0/static/sql-copy.html


.. rubric:: Contents

.. toctree::
   :maxdepth: 2

   usage
   module
   connection
   cursor
   advanced
   extensions
   tz
   pool
   extras
   errorcodes
   faq


.. ifconfig:: builder != 'text'

    .. rubric:: Indices and tables

    * :ref:`genindex`
    * :ref:`search`


.. ifconfig:: todo_include_todos

    .. note::

        **To Do items in the documentation**

        .. todolist::

