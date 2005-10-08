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

# Enable generators on Python 2.2
from __future__ import generators

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

import os
import sys
import shutil
from distutils.core import setup, Extension
from distutils.errors import DistutilsFileError
from distutils.command.build_ext import build_ext
from distutils.sysconfig import get_python_inc
from distutils.ccompiler import get_default_compiler

PSYCOPG_VERSION = '2.0rc1'
version_flags   = []

# to work around older distutil limitations
if sys.version < '2.2.3':
    from distutils.dist import DistributionMetadata
    DistributionMetadata.classifiers = None
    DistributionMetadata.download_url = None

class psycopg_build_ext(build_ext):
    """Conditionally complement the setup.cfg options file.
    
    This class configures the include_dirs, libray_dirs, libraries
    options as required by the system. Most of the configuration happens
    in finalize_options() method.
    
    If you want to set up the build step for a peculiar platform, add a
    method finalize_PLAT(), where PLAT matches your sys.platform.
    """
    user_options = build_ext.user_options[:]
    user_options.extend([
        ('pgdir=', None, 
         "The postgresql directory, either source or bin"),
        ('use-pg-dll', None, 
         "Build against libpq.dll"),
        ('use-pydatetime', None,
         "Use Python datatime objects for date and time representation.")
    ])
    
    boolean_options = build_ext.boolean_options[:]
    boolean_options.extend(('use-pg-dll', 'use-pydatetime'))
    
    # libpq directory in win32 source distribution: compiler dependant.
    libpqdir = None
    
    def initialize_options(self):
        build_ext.initialize_options(self)
        self.use_pydatetime = 1
        self.use_pg_dll = 1
        self.pgdir = None
        
    def get_compiler(self):
        """Return the c compiler to compile extensions.

        If a compiler was not explicitely set (on the command line, for
        example), fall back on the default compiler.
        """
        return self.compiler or get_default_compiler()

    def build_extensions(self):
        # Linking against this library causes psycopg2 to crash 
        # on Python >= 2.4. Maybe related to strdup calls, cfr.
        # http://mail.python.org/pipermail/distutils-sig/2005-April/004433.html
        if self.get_compiler().compiler_type == "mingw32" \
        and 'msvcr71' in self.compiler.dll_libraries:
            self.compiler.dll_libraries.remove('msvcr71')
        
        build_ext.build_extensions(self)
        
    def finalize_win32(self):
        """Finalize build system configuration on win32 platform.
        
        Address issues related to the different environmental configurations
        that can be met:
        
          * msvc or gcc derived (mingw, cygwin) compiler;
          * source or bin PostgreSQL installation
          * static or dynamic linking vs. libpq.dll
        """
        self.include_dirs.append(".")
        self.libraries.append("ws2_32")
        
        if self.build_from_src():
            self.include_dirs.append(os.path.join(
                self.pgdir, "src", "interfaces", "libpq"))
            self.include_dirs.append(os.path.join(
                self.pgdir, "src", "include"))
                
            if self.get_compiler() == "msvc":
                self.libpqdir = os.path.join(
                    self.pgdir, "src", "interfaces", "libpq", "Release")
            else:
                self.libpqdir = os.path.join(
                    self.pgdir, "src", "interfaces", "libpq")
            
            self.library_dirs.append(self.libpqdir)
            
        else:
            self.include_dirs.append(os.path.join(
                self.pgdir, "include"))
            self.library_dirs.append(os.path.join(
                self.pgdir, "lib"))
                
        # Remove the library that was added in the finalize_options, since:
        #  1) We do not need it because we are using the DLL version, or
        #  2) We do need it, but it might need to be msvc'ified, and
        #  3) The other benefit is that msvc needs pqdll to be the last
        self.libraries.remove("pq")

        if self.use_pg_dll:
            self.libraries.append(self.get_lib("pqdll"))
        else:
            self.libraries.append("advapi32")

            if self.get_compiler() == "msvc":
                self.libraries.append("shfolder")
            self.libraries.append(self.get_lib("pq"))

    def finalize_darwin(self):
        """Finalize build system configuration on darwin platform.
        
        fink installs lots of goodies in /sw/... - make sure we check there
        """
        self.include_dirs.append("/sw/include/postgresql")
        self.library_dirs.append("/sw/lib")
        self.include_dirs.append("/opt/local/include/postgresql")
        self.library_dirs.append("/opt/local/lib")
        self.library_dirs.append("/usr/lib")
        self.libraries.append('ssl')
        self.libraries.append('crypto')
        
    def finalize_options(self):
        """Complete the build system configuation."""
        build_ext.finalize_options(self)
        
        self.libraries.append("pq")
        
        if hasattr(self, "finalize_" + sys.platform):
            getattr(self, "finalize_" + sys.platform)()
            
    def run(self):
        build_ext.run(self)
        
        # Add libpq.dll to the lib directory. libpq.dll can be in a wide
        # variety of places. The easiest way to add it to the distribution is
        # to have a copy in a fixed place (lib).
        if sys.platform == 'win32' and self.use_pg_dll:
            shutil.copy(self.find_libpq_dll(), "lib")
        
    def build_from_src(self):
        """Detect if building from postgres source or bin on w32 platform"""
        return os.path.exists(os.path.join(self.pgdir, "src"))

    def get_lib(self, name):
        """Return the full library name given its suffix.
        
        MS VC compiler doesn't assume a "lib" prefix as gcc derived ones do.
        Return the library name as required by the compiler.
        """
        if sys.platform == 'win32' and self.get_compiler() == "msvc":
            return "lib" + name
        else:
            return name
                
    __libpqdll = None
    
    def find_libpq_dll(self, *dirs):
        """Return the full libpq.dll path and name."""
        if self.__libpqdll:
            return self.__libpqdll
            
        for path in self.__iter_libpq_dll():
            if os.path.exists(path):
                sys.stdout.write("libpq.dll found in %s\n" 
                                 % os.path.dirname(path))
                self.__libpqdll = path
                return path
            
        raise DistutilsFileError("Can't find libpq.dll on the system")
        
    def __iter_libpq_dll(self):
        """Iterate on the possible filenames for libpq.dll."""
        # in the build dir for src installation
        if self.libpqdir:
            yield os.path.join(self.libpqdir, "libpq.dll")
            
        # somewhere in the path - probably in system32 - for bin dist
        for path in os.environ['PATH'].split(os.pathsep):
            yield os.path.join(path, "libpq.dll")

# let's start with macro definitions (the ones not already in setup.cfg)
define_macros = []
include_dirs = []

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

# sources

sources = [
    'psycopgmodule.c', 'pqpath.c',  'typecast.c',
    'microprotocols.c', 'microprotocols_proto.c', 
    'connection_type.c', 'connection_int.c', 'cursor_type.c', 'cursor_int.c',
    'adapter_qstring.c', 'adapter_pboolean.c', 'adapter_binary.c',
    'adapter_asis.c', 'adapter_list.c']

from ConfigParser import ConfigParser
parser = ConfigParser()
parser.read('setup.cfg')

# Choose a datetime module
have_pydatetime = False
have_mxdatetime = False
use_pydatetime  = int(parser.get('build_ext', 'use_pydatetime'))

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
    def e(msg):
        sys.stderr.write("error: " + msg + "\n")
    e("psycopg requires a datetime module:")
    e("    mx.DateTime module not found")
    e("    python datetime module not found")
    e("Note that psycopg needs the module headers and not just the module")
    e("itself. If you installed Python or mx.DateTime from a binary package")
    e("you probably need to install its companion -dev or -devel package.")
    sys.exit(1)

# generate a nice version string to avoid confusion when users report bugs
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


if sys.platform == 'win32' and int(parser.get('build_ext', 'use_pg_dll')):
    data_files.append((".\\lib\\site-packages\\psycopg2\\",
        [ "lib\\libpq.dll" ]))
    
# build the extension

sources = map(lambda x: os.path.join('psycopg', x), sources)

ext.append(Extension("psycopg2._psycopg", sources,
                     define_macros=define_macros,
                     include_dirs=include_dirs,
                     undef_macros=[]))

setup(name="psycopg2",
      version=PSYCOPG_VERSION,
      maintainer="Federico Di Gregorio",
      maintainer_email="fog@initd.org",
      author="Federico Di Gregorio",
      author_email="fog@initd.org",
      url="http://initd.org/software/initd/psycopg2",
      download_url = "http://initd.org/software/initd/psycopg2",
      license="GPL or ZPL",
      platforms = ["any"],
      description=__doc__.split("\n")[0],
      long_description="\n".join(__doc__.split("\n")[2:]),
      classifiers=filter(None, classifiers.split("\n")),
      data_files=data_files,
      package_dir={'psycopg2':'lib'},
      packages=['psycopg2'],
      cmdclass={ 'build_ext': psycopg_build_ext },
      ext_modules=ext)
