Introduction
============

.. sectionauthor:: Daniele Varrazzo <daniele.varrazzo@gmail.com>

Psycopg is a PostgreSQL_ adapter for the Python_ programming language. It is a
wrapper for the libpq_, the official PostgreSQL client library.

The `psycopg2` package is the current mature implementation of the adapter: it
is a C extension and as such it is only compatible with CPython_. If you want
to use Psycopg on a different Python implementation (PyPy, Jython, IronPython)
there is an experimental `porting of Psycopg for Ctypes`__, but it is not as
mature as the C implementation yet.

The current `!psycopg2` implementation supports:

..
    NOTE: keep consistent with setup.py and the /features/ page.

- Python 2 versions from 2.6 to 2.7
- Python 3 versions from 3.2 to 3.6
- PostgreSQL server versions from 7.4 to 9.6
- PostgreSQL client library version from 9.1

.. _PostgreSQL: http://www.postgresql.org/
.. _Python: http://www.python.org/
.. _libpq: http://www.postgresql.org/docs/current/static/libpq.html
.. _CPython: http://en.wikipedia.org/wiki/CPython
.. _Ctypes: http://docs.python.org/library/ctypes.html
.. __: https://github.com/mvantellingen/psycopg2-ctypes



.. index::
    single: Install; from PyPI

Binary install from PyPI
------------------------

`!psycopg2` is `available on PyPI`__ in the form of wheel_ packages for the
most common platform (Linux, OSX, Windows): this should make you able to
install a binary version of the module including all the dependencies simply
using:

.. code-block:: console

    $ pip install psycopg2

Make sure to use an up-to-date version of :program:`pip` (you can upgrade it
using something like ``pip install -U pip``)

.. __: PyPI_
.. _PyPI: https://pypi.python.org/pypi/psycopg2/
.. _wheel: http://pythonwheels.com/

.. note::

    The binary packages come with their own versions of a few C libraries,
    among which libpq and libssl, which will be used regardless of other
    libraries available on the client: upgrading the system libraries will not
    upgrade the libraries used by `!psycopg2`. Please build `!psycopg2` from
    source if you want to maintain binary upgradeability.

If you prefer to use the system libraries available on your client you can use
the :command:`pip` ``--no-binary`` option:

.. code-block:: console

    $ pip install --no-binary psycopg2

which can be specified in your :file:`requirements.txt` files too, e.g. use:

.. code-block:: none

    psycopg2>=2.7,<2.8 --no-binary :all:

to use the last bugfix release of the `!psycopg2` 2.7 package, specifying to
always compile it from source. Of course in this case you will have to meet
the :ref:`build prerequisites <build-prerequisites>`.



.. index::
    single: Install; from source

.. _install-from-source:

Install from source
-------------------

.. _source-package:

You can download a copy of Psycopg source files from the `Psycopg download
page`__ or from PyPI_.

.. __: http://initd.org/psycopg/download/



.. _build-prerequisites:

Build prerequisites
^^^^^^^^^^^^^^^^^^^

These notes illustrate how to compile Psycopg on Linux. If you want to compile
Psycopg on other platforms you may have to adjust some details accordingly.

Psycopg is a C wrapper to the libpq PostgreSQL client library. To install it
from sources you will need:

- A C compiler.

- The Python header files. They are usually installed in a package such as
  **python-dev**. A message such as *error: Python.h: No such file or
  directory* is an indication that the Python headers are missing.

- The libpq header files. They are usually installed in a package such as
  **libpq-dev**. If you get an *error: libpq-fe.h: No such file or directory*
  you are missing them.

- The :program:`pg_config` program: it is usually installed by the
  **libpq-dev** package but sometimes it is not in a :envvar:`PATH` directory.
  Having it in the :envvar:`PATH` greatly streamlines the installation, so try
  running ``pg_config --version``: if it returns an error or an unexpected
  version number then locate the directory containing the :program:`pg_config`
  shipped with the right libpq version (usually
  ``/usr/lib/postgresql/X.Y/bin/``) and add it to the :envvar:`PATH`:

  .. code-block:: console

    $ export PATH=/usr/lib/postgresql/X.Y/bin/:$PATH

  You only need :program:`pg_config` to compile `!psycopg2`, not for its
  regular usage.


Runtime requirements
^^^^^^^^^^^^^^^^^^^^

Unless you compile `!psycopg2` as a static library, or you install it from a
self-contained wheel package, it will need the libpq_ library at runtime
(usually distributed in a ``libpq.so`` or ``libpq.dll`` file).  `!psycopg2`
relies on the host OS to find the library if the library is installed in a
standard location there is usually no problem; if the library is in a
non-standard location you will have to tell somehow Psycopg how to find it,
which is OS-dependent (for instance setting a suitable
:envvar:`LD_LIBRARY_PATH` on Linux).

.. note::

    The libpq header files used to compile `!psycopg2` should match the
    version of the library linked at runtime. If you get errors about missing
    or mismatching libraries when importing `!psycopg2` check (e.g. using
    :program:`ldd`) if the module ``psycopg2/_psycopg.so`` is linked to the
    right ``libpq.so``.

.. note::

    Whatever version of libpq `!psycopg2` is compiled with, it will be
    possible to connect to PostgreSQL servers of any supported version: just
    install the most recent libpq version or the most practical, without
    trying to match it to the version of the PostgreSQL server you will have
    to connect to.



.. index::
    single: setup.py
    single: setup.cfg

Non-standard builds
^^^^^^^^^^^^^^^^^^^

If you have less standard requirements such as:

- creating a :ref:`debug build <debug-build>`,
- using :program:`pg_config` not in the :envvar:`PATH`,
- supporting ``mx.DateTime``,

then take a look at the ``setup.cfg`` file.

Some of the options available in ``setup.cfg`` are also available as command
line arguments of the ``build_ext`` sub-command. For instance you can specify
an alternate :program:`pg_config` location using:

.. code-block:: console

    $ python setup.py build_ext --pg-config /path/to/pg_config build

Use ``python setup.py build_ext --help`` to get a list of the options
supported.


.. index::
    single: debug
    single: PSYCOPG_DEBUG

.. _debug-build:

Creating a debug build
^^^^^^^^^^^^^^^^^^^^^^

In case of problems, Psycopg can be configured to emit detailed debug
messages, which can be very useful for diagnostics and to report a bug. In
order to create a debug package:

- `Download`__ and unpack the Psycopg source package.

- Edit the ``setup.cfg`` file adding the ``PSYCOPG_DEBUG`` flag to the
  ``define`` option.

- :ref:`Compile and install <source-package>` the package.

- Set the :envvar:`PSYCOPG_DEBUG` environment variable:

.. code-block:: console

    $ export PSYCOPG_DEBUG=1

- Run your program (making sure that the `!psycopg2` package imported is the
  one you just compiled and not e.g. the system one): you will have a copious
  stream of informations printed on stderr.

.. __: http://initd.org/psycopg/download/



.. index::
    single: tests

.. _test-suite:

Running the test suite
----------------------

Once `!psycopg2` is installed you can run the test suite to verify it is
working correctly. You can run:

.. code-block:: console

    $ python -c "from psycopg2 import tests; tests.unittest.main(defaultTest='tests.test_suite')" --verbose

The tests run against a database called ``psycopg2_test`` on UNIX socket and
the standard port. You can configure a different database to run the test by
setting the environment variables:

- :envvar:`PSYCOPG2_TESTDB`
- :envvar:`PSYCOPG2_TESTDB_HOST`
- :envvar:`PSYCOPG2_TESTDB_PORT`
- :envvar:`PSYCOPG2_TESTDB_USER`

The database should already exist before running the tests.



.. _other-problems:

If you still have problems
--------------------------

Try the following. *In order:*

- Read again the :ref:`build-prerequisites`.

- Read the :ref:`FAQ <faq-compile>`.

- Google for `!psycopg2` *your error message*. Especially useful the week
  after the release of a new OS X version.

- Write to the `Mailing List`__.

- Complain on your blog or on Twitter that `!psycopg2` is the worst package
  ever and about the quality time you have wasted figuring out the correct
  :envvar:`ARCHFLAGS`. Especially useful from the Starbucks near you.

.. __: https://lists.postgresql.org/mj/mj_wwwusr?func=lists-long-full&extra=psycopg
