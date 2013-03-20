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

- Python 2 versions from 2.5 to 2.7
- Python 3 versions from 3.1 to 3.3
- PostgreSQL versions from 7.4 to 9.2

.. _PostgreSQL: http://www.postgresql.org/
.. _Python: http://www.python.org/
.. _libpq: http://www.postgresql.org/docs/current/static/libpq.html
.. _CPython: http://en.wikipedia.org/wiki/CPython
.. _Ctypes: http://docs.python.org/library/ctypes.html
.. __: https://github.com/mvantellingen/psycopg2-ctypes


.. note::

    `!psycopg2` usually depends at runtime on the libpq dynamic library.
    However it can connect to PostgreSQL servers of any supported version,
    independently of the version of the libpq used: just install the most
    recent libpq version or the most practical, without trying to match it to
    the version of the PostgreSQL server you will have to connect to.


Installation
============

If possible, and usually it is, please :ref:`install Psycopg from a package
<install-from-package>` available for your distribution or operating system.

Compiling from source is a very easy task, however `!psycopg2` is a C
extension module and as such it requires a few more things in place respect to
a pure Python module. So, if you don't have experience compiling Python
extension packages, *above all if you are a Windows or a Mac OS user*, please
use a pre-compiled package and go straight to the :ref:`module usage <usage>`
avoid bothering with the gory details.



.. _install-from-package:

Install from a package
----------------------

.. index::
    pair: Install; Linux

**Linux**
    Psycopg is available already packaged in many Linux distributions: look
    for a package such as ``python-psycopg2`` using the package manager of
    your choice.

    On Debian, Ubuntu and other deb-based distributions you should just need::

        sudo apt-get install python-psycopg2

    to install the package with all its dependencies.


.. index::
    pair: Install; Mac OS X

**Mac OS X**
    Psycopg is available as a `fink package`__ in the *unstable* tree: you may
    install it with::

        fink install psycopg2-py27

    .. __: http://pdb.finkproject.org/pdb/package.php/psycopg2-py27

    The library is also available on `MacPorts`__ try::

         sudo port install py27-psycopg2

    .. __: http://www.macports.org/


.. index::
    pair: Install; Windows

**Microsoft Windows**
    Jason Erickson maintains a packaged `Windows port of Psycopg`__ with
    installation executable. Download. Double click. Done.

    .. __: http://www.stickpeople.com/projects/python/win-psycopg/



.. index::
    single: Install; from source

.. _install-from-source:

Install from source
-------------------

These notes illustrate how to compile Psycopg on Linux. If you want to compile
Psycopg on other platforms you may have to adjust some details accordingly.

.. _requirements:

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
  ``/usr/lib/postgresql/X.Y/bin/``) and add it to the :envvar:`PATH`::

    $ export PATH=/usr/lib/postgresql/X.Y/bin/:$PATH
    
  You only need it to compile and install `!psycopg2`, not for its regular
  usage.

.. note::

    The libpq header files used to compile `!psycopg2` should match the
    version of the library linked at runtime. If you get errors about missing
    or mismatching libraries when importing `!psycopg2` check (e.g. using
    :program:`ldd`) if the module ``psycopg2/_psycopg.so`` is linked to the
    right ``libpq.so``.



.. index::
    single: Install; from PyPI

.. _package-manager:

Use a Python package manager
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If the above requirements are satisfied, you can use :program:`easy_install`,
:program:`pip` or whatever the Python package manager of the week::

    $ pip install psycopg2

Please refer to your package manager documentation about performing a local or
global installation, :program:`virtualenv` (fully supported by recent Psycopg
versions), using different Python versions and other nuances.


.. index::
    single: setup.py
    single: setup.cfg

.. _source-package:

Use the source package
^^^^^^^^^^^^^^^^^^^^^^

You can download a copy of Psycopg source files from the `Psycopg download
page`__. Once unpackaged, to compile and install the package you can run::

    $ python setup.py build
    $ sudo python setup.py install

If you have less standard requirements such as:

- creating a :ref:`debug build <debug-build>`,
- using :program:`pg_config` not in the :envvar:`PATH`,
- supporting ``mx.DateTime``,

then take a look at the ``setup.cfg`` file.

Some of the options available in ``setup.cfg`` are also available as command
line arguments of the ``build_ext`` sub-command. For instance you can specify
an alternate :program:`pg_config` version using::

    $ python setup.py build_ext --pg-config /path/to/pg_config build

Use ``python setup.py build_ext --help`` to get a list of the options
supported.

.. __: http://initd.org/psycopg/download/



.. index::
    single: debug
    single: PSYCOPG_DEBUG

.. _debug-build:

Creating a debug build
----------------------

In case of problems, Psycopg can be configured to emit detailed debug
messages, which can be very useful for diagnostics and to report a bug. In
order to create a debug package:

- `Download`__ and unpack the Psycopg source package.

- Edit the ``setup.cfg`` file adding the ``PSYCOPG_DEBUG`` flag to the
  ``define`` option.

- :ref:`Compile and install <source-package>` the package.

- Set the :envvar:`PSYCOPG_DEBUG` variable::

    $ export PSYCOPG_DEBUG=1

- Run your program (making sure that the `!psycopg2` package imported is the
  one you just compiled and not e.g. the system one): you will have a copious
  stream of informations printed on stdout.

.. __: http://initd.org/psycopg/download/



.. _other-problems:

If you still have problems
--------------------------

Try the following. *In order:*

- Read again the :ref:`requirements <requirements>`.

- Read the :ref:`FAQ <faq-compile>`.

- Google for `!psycopg2` *your error message*. Especially useful the week
  after the release of a new OS X version.

- Write to the `Mailing List`__.

- Complain on your blog or on Twitter that `!psycopg2` is the worst package
  ever and about the quality time you have wasted figuring out the correct
  :envvar:`ARCHFLAGS`. Especially useful from the Starbucks near you.

.. __: http://mail.postgresql.org/mj/mj_wwwusr/domain=postgresql.org?func=lists-long-full&extra=psycopg

