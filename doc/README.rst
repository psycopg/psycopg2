How to build psycopg documentation
----------------------------------

Building the documentation usually requires building the library too for
introspection, so you will need the same prerequisites_.  The only extra
prerequisite is virtualenv_: the packages needed to build the docs will be
installed when building the env.

.. _prerequisites: https://www.psycopg.org/docs/install.html#install-from-source
.. _virtualenv: https://virtualenv.pypa.io/en/latest/

Build the env once with::

    make env

Then you can build the documentation with::

    make

You should find the rendered documentation in the ``html`` directory.

Troubleshooting
----------------------------------
1. Outdated pip version
If you get warning of having old pip version be sure to upgrade it.
>WARNING: You are using pip version 20.2.4; however, version 20.3.1 is available.
>You should consider upgrading via the 'PATH_TO_ROOT_FOLDER/psycopg2-2.8.6/doc/env/bin/python -m pip install --upgrade pip' command.

TO fix it and upgrade it successful just follow what the warning message says, i.e run this command on the terminal `PATH_TO_ROOT_FOLDER/psycopg2-2.8.6/doc/env/bin/python -m pip install --upgrade pip`

2. Missing "psycopg.pth" file
When running `make env` and encounter this error, it most likely it is picking wrong non-existent version of python. In that case you need to tell it what is the correct version that is active. The error message looks like this

>echo "$(pwd)/../build/lib.2.7" \
>		> env/lib/python2.7/site-packages/psycopg.pth
>/bin/sh: env/lib/python2.7/site-packages/psycopg.pth: No such file or directory
>make: *** [env] Error 1

To specify your active Python version you have to define `PYTHON_VERSION` environment variable. For example in Unix with let say Python 3.9 active it goes like:
`export PYTHON_VERSION=3` then you can happily run `make env` and the error should be gone
