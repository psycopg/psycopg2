# setup.py - distutils packaging
#
# Copyright (C) 2003-2004 Federico Di Gregorio  <fog@debian.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
"""Python-PostgreSQL Database Adapter
 
psycopg is a PostgreSQL database adapter for the Python programming
language. This is version 2, a complete rewrite of the original code to
provide new-style classes for connection and cursor objects and other sweet
candies. Like the original, psycopg 2 was written with the aim of being
very small and fast, and stable as a rock.
 
psycopg is different from the other database adapter because it was
designed for heavily multi-threaded applications that create and destroy
lots of cursors and make a conspicuous number of concurrent INSERTs or
UPDATEs. psycopg 2 also provide full asycronous operations for the really
brave programmer.
"""

classifiers = """\
Development Status :: 4 - Beta
Intended Audience :: Developers
License :: OSI Approved :: GNU General Public License (GPL)
License :: OSI Approved :: Zope Public License
Programming Language :: Python
Programming Language :: C
Programming Language :: SQL
Topic :: Database
Topic :: Database :: Front-Ends
Topic :: Software Development
Topic :: Software Development :: Libraries :: Python Modules
Operating System :: Microsoft :: Windows
Operating System :: Unix
"""

import sys, os.path
from distutils.core import setup, Extension
from distutils.sysconfig import get_python_inc
import distutils.ccompiler

PSYCOPG_VERSION = '1.99.13/devel'
version_flags   = []

have_pydatetime = False
have_mxdatetime = False
use_pydatetime  = True

# windows-only definitions (TODO: this should be moved to setup.cfg!)
POSTGRESQLDIR = "D:\\POSTGRESQL-7.4.2"
USE_PG_DLL = True

# to work around older distutil limitations
if sys.version < '2.2.3':
  from distutils.dist import DistributionMetadata
  DistributionMetadata.classifiers = None
  DistributionMetadata.download_url = None

# let's start with macro definitions (the ones not already in setup.cfg)
define_macros = []

# python version
define_macros.append(('PY_MAJOR_VERSION', str(sys.version_info[0])))
define_macros.append(('PY_MINOR_VERSION', str(sys.version_info[1])))

# some macros related to python versions and features
if sys.version_info[0] >= 2 and sys.version_info[1] >= 3:
    define_macros.append(('HAVE_PYBOOL','1'))
if sys.version_info[0] >= 2 and sys.version_info[1] >= 4:
    define_macros.append(('HAVE_DECIMAL','1'))

# gather information to build the extension module
ext = [] ; data_files = []
library_dirs = [] ; libraries = [] ; include_dirs = []

if sys.platform == 'win32':
    include_dirs = ['.',
                    POSTGRESQLDIR + "\\src\\interfaces\\libpq",
                    POSTGRESQLDIR + "\\src\\include" ]
    library_dirs = [ POSTGRESQLDIR + "\\src\\interfaces\\libpq\\Release" ]
    libraries = ["ws2_32"]
    if USE_PG_DLL:
        data_files.append((".\\lib\site-packages\\",
            [POSTGRESQLDIR + "\\src\interfaces\\libpq\\Release\\libpq.dll"]))
        libraries.append("libpqdll")
    else:
        libraries.append("libpq")
        libraries.append("advapi32")

# extra checks on darwin
if sys.platform == "darwin":
    # fink installs lots of goodies in /sw/... - make sure we check there
    include_dirs.append("/sw/include/postgresql")
    library_dirs.append("/sw/lib")
    include_dirs.append("/opt/local/include/postgresql")
    library_dirs.append("/opt/local/lib")
    library_dirs.append("/usr/lib")
    libraries.append('ssl')
    libraries.append('crypto')

# sources

sources = [
    'psycopgmodule.c', 'pqpath.c',  'typecast.c',
    'microprotocols.c', 'microprotocols_proto.c', 
    'connection_type.c', 'connection_int.c', 'cursor_type.c', 'cursor_int.c',
    'adapter_qstring.c', 'adapter_pboolean.c', 'adapter_binary.c',
    'adapter_asis.c']

# check for mx package
mxincludedir = os.path.join(get_python_inc(plat_specific=1), "mx")
if os.path.exists(mxincludedir):
    include_dirs.append(mxincludedir)
    define_macros.append(('HAVE_MXDATETIME','1'))
    sources.append('adapter_mxdatetime.c')
    have_mxdatetime = True
    version_flags.append('mx')

# check for python datetime package
if os.path.exists(os.path.join(get_python_inc(plat_specific=1),"datetime.h")):
    define_macros.append(('HAVE_PYDATETIME','1'))
    sources.append('adapter_datetime.c')
    have_pydatetime = True
    version_flags.append('dt')
    
# now decide which package will be the default for date/time typecasts
if have_pydatetime and use_pydatetime \
       or have_pydatetime and not have_mxdatetime:
    define_macros.append(('PSYCOPG_DEFAULT_PYDATETIME','1'))
elif have_mxdatetime:
    define_macros.append(('PSYCOPG_DEFAULT_MXDATETIME','1'))
else:    
    sys.stderr.write("error: psycopg requires a datetime module:\n")
    sys.stderr.write("error:     mx.DateTime module not found\n")
    sys.stderr.write("error:     python datetime module not found\n")
    sys.exit(1)

# generate a nice version string to avoid confusion when users report bugs
from ConfigParser import ConfigParser
parser = ConfigParser()
parser.read('setup.cfg')
for have in parser.get('build_ext', 'define').split(','):
    if have == 'PSYCOPG_EXTENSIONS':
        version_flags.append('ext')
    elif have == 'HAVE_PQPROTOCOL3':
        version_flags.append('pq3')
if version_flags:
    PSYCOPG_VERSION_EX = PSYCOPG_VERSION + " (%s)" % ' '.join(version_flags)
else:
    PSYCOPG_VERSION_EX = PSYCOPG_VERSION
    
if sys.platform != 'win32':
    define_macros.append(('PSYCOPG_VERSION', '"'+PSYCOPG_VERSION_EX+'"'))
else:
    define_macros.append(('PSYCOPG_VERSION', '\\"'+PSYCOPG_VERSION_EX+'\\"'))


# build the extension

sources = map(lambda x: os.path.join('psycopg', x), sources)

ext.append(Extension("psycopg._psycopg", sources,
                     include_dirs=include_dirs,
                     library_dirs=library_dirs,
                     define_macros=define_macros,
                     undef_macros=[],
                     libraries=libraries))

setup(name="psycopg",
      version=PSYCOPG_VERSION,
      maintainer="Federico Di Gregorio",
      maintainer_email="fog@initd.org",
      author="Federico Di Gregorio",
      author_email="fog@initd.org",
      url="http://initd.org/software/initd/psycopg",
      download_url = "http://initd.org/software/initd/psycopg",
      license="GPL or ZPL",
      platforms = ["any"],
      description=__doc__.split("\n")[0],
      long_description="\n".join(__doc__.split("\n")[2:]),
      classifiers=filter(None, classifiers.split("\n")),
      data_files=data_files,
      package_dir={'psycopg':'lib'},
      packages=['psycopg'],
      ext_modules=ext)
