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

import os
import os.path
import sys
import popen2
import ConfigParser
from distutils.core import setup, Extension
from distutils.errors import DistutilsFileError
from distutils.command.build_ext import build_ext
from distutils.sysconfig import get_python_inc
from distutils.ccompiler import get_default_compiler

PSYCOPG_VERSION = '2.0.8'
version_flags   = []

PLATFORM_IS_WINDOWS = sys.platform.lower().startswith('win')

# to work around older distutil limitations
if sys.version < '2.2.3':
    from distutils.dist import DistributionMetadata
    DistributionMetadata.classifiers = None
    DistributionMetadata.download_url = None

def get_pg_config(kind, pg_config="pg_config"):
    if ' ' in pg_config:
        pg_config = '"'+pg_config+'"'
    p = popen2.popen3(pg_config + " --" + kind)
    r = p[0].readline().strip()
    if not r:
        raise Warning(p[2].readline())
    return r

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
        ('use-pydatetime', None,
         "Use Python datatime objects for date and time representation."),
        ('pg-config=', None,
          "The name of the pg_config binary and/or full path to find it"),
        ('use-decimal', None,
         "Use Decimal type even on Python 2.3 if the module is provided."),
        ('have-ssl', None,
         "Compile with OpenSSL built PostgreSQL libraries (Windows only)."),
    ])

    boolean_options = build_ext.boolean_options[:]
    boolean_options.extend(('use-pydatetime', 'use-decimal', 'have-ssl'))

    DEFAULT_PG_CONFIG = "pg_config"

    def initialize_options(self):
        build_ext.initialize_options(self)
        self.use_pg_dll = 1
        self.pgdir = None
        self.mx_include_dir = None

        self.pg_config = self.autodetect_pg_config_path()

    def get_compiler(self):
        """Return the name of the C compiler used to compile extensions.

        If a compiler was not explicitely set (on the command line, for
        example), fall back on the default compiler.
        """
        if self.compiler:
            # distutils doesn't keep the type of self.compiler uniform; we
            # compensate:
            if isinstance(self.compiler, str):
                name = self.compiler
            else:
                name = self.compiler.compiler_type
        else:
            name = get_default_compiler()
        return name

    def get_pg_config(self, kind):
        return get_pg_config(kind, self.pg_config)

    def finalize_win32(self):
        """Finalize build system configuration on win32 platform."""
        import struct
        sysVer = sys.version_info[:2]

        # Add compiler-specific arguments:
        extra_compiler_args = []

        compiler_name = self.get_compiler().lower()
        compiler_is_msvc = compiler_name.startswith('msvc')
        compiler_is_mingw = compiler_name.startswith('mingw')
        if compiler_is_msvc:
            # If we're using MSVC 7.1 or later on a 32-bit platform, add the
            # /Wp64 option to generate warnings about Win64 portability
            # problems.
            if sysVer >= (2,4) and struct.calcsize('P') == 4:
                extra_compiler_args.append('/Wp64')
        elif compiler_is_mingw:
            # Default MinGW compilation of Python extensions on Windows uses
            # only -O:
            extra_compiler_args.append('-O3')

            # GCC-compiled Python on non-Windows platforms is built with strict
            # aliasing disabled, but that must be done explicitly on Windows to
            # avoid large numbers of warnings for perfectly idiomatic Python C
            # API code.
            extra_compiler_args.append('-fno-strict-aliasing')

            # Force correct C runtime library linkage:
            if sysVer <= (2,3):
                # Yes:  'msvcr60', rather than 'msvcrt', is the correct value
                # on the line below:
                self.libraries.append('msvcr60')
            elif sysVer in ((2,4), (2,5)):
                self.libraries.append('msvcr71')
            # Beyond Python 2.5, we take our chances on the default C runtime
            # library, because we don't know what compiler those future
            # versions of Python will use.

        for exten in ext: # ext is a global list of Extension objects
            exten.extra_compile_args.extend(extra_compiler_args)
        # End of add-compiler-specific arguments section.

        self.libraries.append("ws2_32")
        self.libraries.append("advapi32")
        if compiler_is_msvc:
            # MSVC requires an explicit "libpq"
            self.libraries.remove("pq")
            self.libraries.append("libpq")
            self.libraries.append("shfolder")
            for path in self.library_dirs:
                if os.path.isfile(os.path.join(path, "ms", "libpq.lib")):
                    self.library_dirs.append(os.path.join(path, "ms"))
                    break
            if have_ssl:
                self.libraries.append("libeay32")
                self.libraries.append("ssleay32")
                self.libraries.append("user32")
                self.libraries.append("gdi32")

    def finalize_darwin(self):
        """Finalize build system configuration on darwin platform."""
        self.libraries.append('ssl')
        self.libraries.append('crypto')

    def finalize_options(self):
        """Complete the build system configuation."""
        build_ext.finalize_options(self)

        self.include_dirs.append(".")
        self.libraries.append("pq")

        try:
            self.library_dirs.append(self.get_pg_config("libdir"))
            self.include_dirs.append(self.get_pg_config("includedir"))
            self.include_dirs.append(self.get_pg_config("includedir-server"))
            try:
                # Here we take a conservative approach: we suppose that
                # *at least* PostgreSQL 7.4 is available (this is the only
                # 7.x series supported by psycopg 2)
                pgversion = self.get_pg_config("version").split()[1]
                pgmajor, pgminor, pgpatch = pgversion.split('.')
            except:
                pgmajor, pgminor, pgpatch = 7, 4, 0
            define_macros.append(("PG_MAJOR_VERSION", pgmajor))
            define_macros.append(("PG_MINOR_VERSION", pgminor))
            define_macros.append(("PG_PATCH_VERSION", pgpatch))
        except Warning, w:
            if self.pg_config == self.DEFAULT_PG_CONFIG:
                sys.stderr.write("Warning: %s" % str(w))
            else:
                sys.stderr.write("Error: %s" % str(w))
                sys.exit(1)

        if hasattr(self, "finalize_" + sys.platform):
            getattr(self, "finalize_" + sys.platform)()

    def autodetect_pg_config_path(self):
        res = None

        if PLATFORM_IS_WINDOWS:
            res = self.autodetect_pg_config_path_windows()

        return res or self.DEFAULT_PG_CONFIG

    def autodetect_pg_config_path_windows(self):
        # Find the first PostgreSQL installation listed in the registry and
        # return the full path to its pg_config utility.
        #
        # This autodetection is performed *only* if the following conditions
        # hold:
        #
        # 1) The pg_config utility is not already available on the PATH:
        if os.popen('pg_config').close() is None: # .close()->None == success
            return None
        # 2) The user has not specified any of the following settings in
        #    setup.cfg:
        #     - pg_config
        #     - include_dirs
        #     - library_dirs
        for settingName in ('pg_config', 'include_dirs', 'library_dirs'):
            try:
                val = parser.get('build_ext', settingName)
            except ConfigParser.NoOptionError:
                pass
            else:
                if val.strip() != '':
                    return None
        # end of guard conditions

        import _winreg

        pg_inst_base_dir = None
        pg_config_path = None

        reg = _winreg.ConnectRegistry(None, _winreg.HKEY_LOCAL_MACHINE)
        try:
            pg_inst_list_key = _winreg.OpenKey(reg,
                'SOFTWARE\\PostgreSQL\\Installations'
              )
        except EnvironmentError:
            pg_inst_list_key = None

        if pg_inst_list_key is not None:
            try:
                # Determine the name of the first subkey, if any:
                try:
                    first_sub_key_name = _winreg.EnumKey(pg_inst_list_key, 0)
                except EnvironmentError:
                    first_sub_key_name = None

                if first_sub_key_name is not None:
                    pg_first_inst_key = _winreg.OpenKey(reg,
                        'SOFTWARE\\PostgreSQL\\Installations\\'
                        + first_sub_key_name
                      )
                    try:
                        pg_inst_base_dir = _winreg.QueryValueEx(
                            pg_first_inst_key, 'Base Directory'
                          )[0]
                    finally:
                        _winreg.CloseKey(pg_first_inst_key)
            finally:
                _winreg.CloseKey(pg_inst_list_key)

        if pg_inst_base_dir and os.path.exists(pg_inst_base_dir):
            pg_config_path = os.path.join(pg_inst_base_dir, 'bin',
                'pg_config.exe'
              )
            # Support unicode paths, if this version of Python provides the
            # necessary infrastructure:
            if hasattr(sys, 'getfilesystemencoding'):
                pg_config_path = pg_config_path.encode(
                    sys.getfilesystemencoding()
                  )

        return pg_config_path

# let's start with macro definitions (the ones not already in setup.cfg)
define_macros = []
include_dirs = []

# python version
define_macros.append(('PY_MAJOR_VERSION', str(sys.version_info[0])))
define_macros.append(('PY_MINOR_VERSION', str(sys.version_info[1])))

# some macros related to python versions and features
if sys.version_info[0] >= 2 and sys.version_info[1] >= 3:
    define_macros.append(('HAVE_PYBOOL','1'))

# gather information to build the extension module
ext = [] ; data_files = []

# sources

sources = [
    'psycopgmodule.c', 'pqpath.c',  'typecast.c',
    'microprotocols.c', 'microprotocols_proto.c',
    'connection_type.c', 'connection_int.c', 'cursor_type.c', 'cursor_int.c',
    'lobject_type.c', 'lobject_int.c',
    'adapter_qstring.c', 'adapter_pboolean.c', 'adapter_binary.c',
    'adapter_asis.c', 'adapter_list.c']

parser = ConfigParser.ConfigParser()
parser.read('setup.cfg')

# Choose if to use Decimal type
use_decimal = int(parser.get('build_ext', 'use_decimal'))
if sys.version_info[0] >= 2 and (
    sys.version_info[1] >= 4 or (sys.version_info[1] == 3 and use_decimal)):
    define_macros.append(('HAVE_DECIMAL','1'))
    version_flags.append('dec')

# Choose a datetime module
have_pydatetime = False
have_mxdatetime = False
use_pydatetime  = int(parser.get('build_ext', 'use_pydatetime'))

# check for mx package
if parser.has_option('build_ext', 'mx_include_dir'):
    mxincludedir = parser.get('build_ext', 'mx_include_dir')
else:
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

if not PLATFORM_IS_WINDOWS:
    define_macros.append(('PSYCOPG_VERSION', '"'+PSYCOPG_VERSION_EX+'"'))
else:
    define_macros.append(('PSYCOPG_VERSION', '\\"'+PSYCOPG_VERSION_EX+'\\"'))

if parser.has_option('build_ext', 'have_ssl'):
    have_ssl = int(parser.get('build_ext', 'have_ssl'))
else:
    have_ssl = 0

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
      url="http://initd.org/tracker/psycopg",
      download_url = "http://initd.org/pub/software/psycopg2",
      license="GPL with exceptions or ZPL",
      platforms = ["any"],
      description=__doc__.split("\n")[0],
      long_description="\n".join(__doc__.split("\n")[2:]),
      classifiers=filter(None, classifiers.split("\n")),
      data_files=data_files,
      package_dir={'psycopg2':'lib'},
      packages=['psycopg2'],
      cmdclass={ 'build_ext': psycopg_build_ext },
      ext_modules=ext)

