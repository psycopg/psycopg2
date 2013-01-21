# setup.py - distutils packaging
#
# Copyright (C) 2003-2010 Federico Di Gregorio  <fog@debian.org>
#
# psycopg2 is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# psycopg2 is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.

"""Python-PostgreSQL Database Adapter

psycopg2 is a PostgreSQL database adapter for the Python programming
language.  psycopg2 was written with the aim of being very small and fast,
and stable as a rock.

psycopg2 is different from the other database adapter because it was
designed for heavily multi-threaded applications that create and destroy
lots of cursors and make a conspicuous number of concurrent INSERTs or
UPDATEs. psycopg2 also provide full asycronous operations and support
for coroutine libraries.
"""

classifiers = """\
Development Status :: 5 - Production/Stable
Intended Audience :: Developers
License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)
License :: OSI Approved :: Zope Public License
Programming Language :: Python
Programming Language :: Python :: 3
Programming Language :: C
Programming Language :: SQL
Topic :: Database
Topic :: Database :: Front-Ends
Topic :: Software Development
Topic :: Software Development :: Libraries :: Python Modules
Operating System :: Microsoft :: Windows
Operating System :: Unix
"""

# Note: The setup.py must be compatible with both Python 2 and 3

import os
import sys
import re
import subprocess
from distutils.core import setup, Extension
from distutils.command.build_ext import build_ext
from distutils.sysconfig import get_python_inc
from distutils.ccompiler import get_default_compiler
from distutils.util import get_platform

try:
    from distutils.command.build_py import build_py_2to3 as build_py
except ImportError:
    from distutils.command.build_py import build_py
else:
    # Configure distutils to run our custom 2to3 fixers as well
    from lib2to3.refactor import get_fixers_from_package
    build_py.fixer_names = get_fixers_from_package('lib2to3.fixes')
    build_py.fixer_names.append('fix_b')
    sys.path.insert(0, 'scripts')

try:
    import configparser
except ImportError:
    import ConfigParser as configparser

# Take a look at http://www.python.org/dev/peps/pep-0386/
# for a consistent versioning pattern.

PSYCOPG_VERSION = '2.4.6'

version_flags   = ['dt', 'dec']

PLATFORM_IS_WINDOWS = sys.platform.lower().startswith('win')


class PostgresConfig:
    def __init__(self, build_ext):
        self.build_ext = build_ext
        self.pg_config_exe = self.build_ext.pg_config
        if not self.pg_config_exe:
            self.pg_config_exe = self.autodetect_pg_config_path()
        if self.pg_config_exe is None:
            sys.stderr.write("""\
Error: pg_config executable not found.

Please add the directory containing pg_config to the PATH
or specify the full executable path with the option:

    python setup.py build_ext --pg-config /path/to/pg_config build ...

or with the pg_config option in 'setup.cfg'.
""")
            sys.exit(1)

    def query(self, attr_name):
        """Spawn the pg_config executable, querying for the given config
        name, and return the printed value, sanitized. """
        try:
            pg_config_process = subprocess.Popen(
                [self.pg_config_exe, "--" + attr_name],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE)
        except OSError:
            raise Warning("Unable to find 'pg_config' file in '%s'" %
                          self.pg_config_exe)
        pg_config_process.stdin.close()
        result = pg_config_process.stdout.readline().strip()
        if not result:
            raise Warning(pg_config_process.stderr.readline())
        if not isinstance(result, str):
            result = result.decode('ascii')
        return result

    def find_on_path(self, exename, path_directories=None):
        if not path_directories:
            path_directories = os.environ['PATH'].split(os.pathsep)
        for dir_name in path_directories:
            fullpath = os.path.join(dir_name, exename)
            if os.path.isfile(fullpath):
                return fullpath
        return None

    def autodetect_pg_config_path(self):
        """Find and return the path to the pg_config executable."""
        if PLATFORM_IS_WINDOWS:
            return self.autodetect_pg_config_path_windows()
        else:
            return self.find_on_path('pg_config')

    def autodetect_pg_config_path_windows(self):
        """Attempt several different ways of finding the pg_config
        executable on Windows, and return its full path, if found."""

        # This code only runs if they have not specified a pg_config option
        # in the config file or via the commandline.

        # First, check for pg_config.exe on the PATH, and use that if found.
        pg_config_exe = self.find_on_path('pg_config.exe')
        if pg_config_exe:
            return pg_config_exe

        # Now, try looking in the Windows Registry to find a PostgreSQL
        # installation, and infer the path from that.
        pg_config_exe = self._get_pg_config_from_registry()
        if pg_config_exe:
            return pg_config_exe

        return None

    def _get_pg_config_from_registry(self):
        try:
            import winreg
        except ImportError:
            import _winreg as winreg

        reg = winreg.ConnectRegistry(None, winreg.HKEY_LOCAL_MACHINE)
        try:
            pg_inst_list_key = winreg.OpenKey(reg,
                'SOFTWARE\\PostgreSQL\\Installations')
        except EnvironmentError:
            # No PostgreSQL installation, as best as we can tell.
            return None

        try:
            # Determine the name of the first subkey, if any:
            try:
                first_sub_key_name = winreg.EnumKey(pg_inst_list_key, 0)
            except EnvironmentError:
                return None

            pg_first_inst_key = winreg.OpenKey(reg,
                'SOFTWARE\\PostgreSQL\\Installations\\'
                + first_sub_key_name)
            try:
                pg_inst_base_dir = winreg.QueryValueEx(
                    pg_first_inst_key, 'Base Directory')[0]
            finally:
                winreg.CloseKey(pg_first_inst_key)

        finally:
            winreg.CloseKey(pg_inst_list_key)

        pg_config_path = os.path.join(
            pg_inst_base_dir, 'bin', 'pg_config.exe')
        if not os.path.exists(pg_config_path):
            return None

        # Support unicode paths, if this version of Python provides the
        # necessary infrastructure:
        if sys.version_info[0] < 3 \
        and hasattr(sys, 'getfilesystemencoding'):
            pg_config_path = pg_config_path.encode(
                sys.getfilesystemencoding())

        return pg_config_path


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
        ('have-ssl', None,
         "Compile with OpenSSL built PostgreSQL libraries (Windows only)."),
        ('static-libpq', None,
         "Statically link the PostgreSQL client library"),
    ])

    boolean_options = build_ext.boolean_options[:]
    boolean_options.extend(('use-pydatetime', 'have-ssl', 'static-libpq'))

    def __init__(self, *args, **kwargs):
        build_ext.__init__(self, *args, **kwargs)

    def initialize_options(self):
        build_ext.initialize_options(self)
        self.use_pg_dll = 1
        self.pgdir = None
        self.mx_include_dir = None
        self.use_pydatetime = 1
        self.have_ssl = have_ssl
        self.static_libpq = static_libpq
        self.pg_config = None

    def compiler_is_msvc(self):
        return self.get_compiler_name().lower().startswith('msvc')

    def compiler_is_mingw(self):
        return self.get_compiler_name().lower().startswith('mingw')

    def get_compiler_name(self):
        """Return the name of the C compiler used to compile extensions.

        If a compiler was not explicitly set (on the command line, for
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

    def get_export_symbols(self, extension):
        # Fix MSVC seeing two of the same export symbols.
        if self.compiler_is_msvc():
            return []
        else:
            return build_ext.get_export_symbols(self, extension)

    def build_extension(self, extension):
        build_ext.build_extension(self, extension)
        sysVer = sys.version_info[:2]

        # For Python versions that use MSVC compiler 2008, re-insert the
        # manifest into the resulting .pyd file.
        if self.compiler_is_msvc() and sysVer not in ((2, 4), (2, 5)):
            platform = get_platform()
            # Default to the x86 manifest
            manifest = '_psycopg.vc9.x86.manifest'
            if platform == 'win-amd64':
                manifest = '_psycopg.vc9.amd64.manifest'
            self.compiler.spawn(
                ['mt.exe', '-nologo', '-manifest',
                 os.path.join('psycopg', manifest),
                 '-outputresource:%s;2' % (
                        os.path.join(self.build_lib,
                                     'psycopg2', '_psycopg.pyd'))])

    def finalize_win32(self):
        """Finalize build system configuration on win32 platform."""
        sysVer = sys.version_info[:2]

        # Add compiler-specific arguments:
        extra_compiler_args = []

        if self.compiler_is_mingw():
            # Default MinGW compilation of Python extensions on Windows uses
            # only -O:
            extra_compiler_args.append('-O3')

            # GCC-compiled Python on non-Windows platforms is built with strict
            # aliasing disabled, but that must be done explicitly on Windows to
            # avoid large numbers of warnings for perfectly idiomatic Python C
            # API code.
            extra_compiler_args.append('-fno-strict-aliasing')

            # Force correct C runtime library linkage:
            if sysVer <= (2, 3):
                # Yes:  'msvcr60', rather than 'msvcrt', is the correct value
                # on the line below:
                self.libraries.append('msvcr60')
            elif sysVer in ((2, 4), (2, 5)):
                self.libraries.append('msvcr71')
            # Beyond Python 2.5, we take our chances on the default C runtime
            # library, because we don't know what compiler those future
            # versions of Python will use.

        for extension in ext:  # ext is a global list of Extension objects
            extension.extra_compile_args.extend(extra_compiler_args)
        # End of add-compiler-specific arguments section.

        self.libraries.append("ws2_32")
        self.libraries.append("advapi32")
        if self.compiler_is_msvc():
            # MSVC requires an explicit "libpq"
            self.libraries.remove("pq")
            self.libraries.append("secur32")
            self.libraries.append("libpq")
            self.libraries.append("shfolder")
            for path in self.library_dirs:
                if os.path.isfile(os.path.join(path, "ms", "libpq.lib")):
                    self.library_dirs.append(os.path.join(path, "ms"))
                    break
            if self.have_ssl:
                self.libraries.append("libeay32")
                self.libraries.append("ssleay32")
                self.libraries.append("crypt32")
                self.libraries.append("user32")
                self.libraries.append("gdi32")

    def finalize_darwin(self):
        """Finalize build system configuration on darwin platform."""
        self.libraries.append('ssl')
        self.libraries.append('crypto')

    def finalize_linux2(self):
        """Finalize build system configuration on GNU/Linux platform."""
        # tell piro that GCC is fine and dandy, but not so MS compilers
        for extension in self.extensions:
            extension.extra_compile_args.append(
                '-Wdeclaration-after-statement')

    finalize_linux3 = finalize_linux2

    def finalize_options(self):
        """Complete the build system configuation."""
        build_ext.finalize_options(self)

        pg_config_helper = PostgresConfig(self)

        self.include_dirs.append(".")
        if self.static_libpq:
            if not getattr(self, 'link_objects', None):
                self.link_objects = []
            self.link_objects.append(
                    os.path.join(pg_config_helper.query("libdir"), "libpq.a"))
        else:
            self.libraries.append("pq")

        try:
            self.library_dirs.append(pg_config_helper.query("libdir"))
            self.include_dirs.append(pg_config_helper.query("includedir"))
            self.include_dirs.append(pg_config_helper.query("includedir-server"))
            try:
                # Here we take a conservative approach: we suppose that
                # *at least* PostgreSQL 7.4 is available (this is the only
                # 7.x series supported by psycopg 2)
                pgversion = pg_config_helper.query("version").split()[1]
            except:
                pgversion = "7.4.0"

            verre = re.compile(
                r"(\d+)\.(\d+)(?:(?:\.(\d+))|(devel|(alpha|beta|rc)\d+))")
            m = verre.match(pgversion)
            if m:
                pgmajor, pgminor, pgpatch = m.group(1, 2, 3)
                if pgpatch is None or not pgpatch.isdigit():
                    pgpatch = 0
            else:
                sys.stderr.write(
                    "Error: could not determine PostgreSQL version from '%s'"
                                                                % pgversion)
                sys.exit(1)

            define_macros.append(("PG_VERSION_HEX", "0x%02X%02X%02X" %
                                  (int(pgmajor), int(pgminor), int(pgpatch))))
        except Warning:
            w = sys.exc_info()[1]  # work around py 2/3 different syntax
            sys.stderr.write("Error: %s\n" % w)
            sys.exit(1)

        if hasattr(self, "finalize_" + sys.platform):
            getattr(self, "finalize_" + sys.platform)()


# let's start with macro definitions (the ones not already in setup.cfg)
define_macros = []
include_dirs = []

# gather information to build the extension module
ext = []
data_files = []

# sources

sources = [
    'psycopgmodule.c',
    'green.c', 'pqpath.c', 'utils.c', 'bytes_format.c',

    'connection_int.c', 'connection_type.c',
    'cursor_int.c', 'cursor_type.c',
    'lobject_int.c', 'lobject_type.c',
    'notify_type.c', 'xid_type.c',

    'adapter_asis.c', 'adapter_binary.c', 'adapter_datetime.c',
    'adapter_list.c', 'adapter_pboolean.c', 'adapter_pdecimal.c',
    'adapter_pint.c', 'adapter_pfloat.c', 'adapter_qstring.c',
    'microprotocols.c', 'microprotocols_proto.c',
    'typecast.c',
]

depends = [
    # headers
    'config.h', 'pgtypes.h', 'psycopg.h', 'python.h',
    'connection.h', 'cursor.h', 'green.h', 'lobject.h',
    'notify.h', 'pqpath.h', 'xid.h',

    'adapter_asis.h', 'adapter_binary.h', 'adapter_datetime.h',
    'adapter_list.h', 'adapter_pboolean.h', 'adapter_pdecimal.h',
    'adapter_pint.h', 'adapter_pfloat.h', 'adapter_qstring.h',
    'microprotocols.h', 'microprotocols_proto.h',
    'typecast.h', 'typecast_binary.h',

    # included sources
    'typecast_array.c', 'typecast_basic.c', 'typecast_binary.c',
    'typecast_builtins.c', 'typecast_datetime.c',
]

parser = configparser.ConfigParser()
parser.read('setup.cfg')

# Choose a datetime module
have_pydatetime = True
have_mxdatetime = False
use_pydatetime  = int(parser.get('build_ext', 'use_pydatetime'))

# check for mx package
if parser.has_option('build_ext', 'mx_include_dir'):
    mxincludedir = parser.get('build_ext', 'mx_include_dir')
else:
    mxincludedir = os.path.join(get_python_inc(plat_specific=1), "mx")
if os.path.exists(mxincludedir):
    # Build the support for mx: we will check at runtime if it can be imported
    include_dirs.append(mxincludedir)
    define_macros.append(('HAVE_MXDATETIME', '1'))
    sources.append('adapter_mxdatetime.c')
    depends.extend(['adapter_mxdatetime.h', 'typecast_mxdatetime.c'])
    have_mxdatetime = True
    version_flags.append('mx')

# now decide which package will be the default for date/time typecasts
if have_pydatetime and (use_pydatetime or not have_mxdatetime):
    define_macros.append(('PSYCOPG_DEFAULT_PYDATETIME', '1'))
elif have_mxdatetime:
    define_macros.append(('PSYCOPG_DEFAULT_MXDATETIME', '1'))
else:
    error_message = """\
psycopg requires a datetime module:
    mx.DateTime module not found
    python datetime module not found

Note that psycopg needs the module headers and not just the module
itself. If you installed Python or mx.DateTime from a binary package
you probably need to install its companion -dev or -devel package."""

    for line in error_message.split("\n"):
        sys.stderr.write("error: " + line)
    sys.exit(1)

# generate a nice version string to avoid confusion when users report bugs
version_flags.append('pq3') # no more a choice
for have in parser.get('build_ext', 'define').split(','):
    if have == 'PSYCOPG_EXTENSIONS':
        version_flags.append('ext')
if version_flags:
    PSYCOPG_VERSION_EX = PSYCOPG_VERSION + " (%s)" % ' '.join(version_flags)
else:
    PSYCOPG_VERSION_EX = PSYCOPG_VERSION

if not PLATFORM_IS_WINDOWS:
    define_macros.append(('PSYCOPG_VERSION', '"' + PSYCOPG_VERSION_EX + '"'))
else:
    define_macros.append(('PSYCOPG_VERSION', '\\"' + PSYCOPG_VERSION_EX + '\\"'))

if parser.has_option('build_ext', 'have_ssl'):
    have_ssl = int(parser.get('build_ext', 'have_ssl'))
else:
    have_ssl = 0

if parser.has_option('build_ext', 'static_libpq'):
    static_libpq = int(parser.get('build_ext', 'static_libpq'))
else:
    static_libpq = 0

# build the extension

sources = [ os.path.join('psycopg', x) for x in sources]
depends = [ os.path.join('psycopg', x) for x in depends]

ext.append(Extension("psycopg2._psycopg", sources,
                     define_macros=define_macros,
                     include_dirs=include_dirs,
                     depends=depends,
                     undef_macros=[]))

# Compute the direct download url.
# Note that the current package installation programs are stupidly intelligent
# and will try to install a beta if they find a link in the homepage instead of
# using these pretty metadata. But that's their problem, not ours.
download_url = (
    "http://initd.org/psycopg/tarballs/PSYCOPG-%s/psycopg2-%s.tar.gz"
    % ('-'.join(PSYCOPG_VERSION.split('.')[:2]), PSYCOPG_VERSION))

setup(name="psycopg2",
      version=PSYCOPG_VERSION,
      maintainer="Federico Di Gregorio",
      maintainer_email="fog@initd.org",
      author="Federico Di Gregorio",
      author_email="fog@initd.org",
      url="http://initd.org/psycopg/",
      download_url=download_url,
      license="GPL with exceptions or ZPL",
      platforms=["any"],
      description=__doc__.split("\n")[0],
      long_description="\n".join(__doc__.split("\n")[2:]),
      classifiers=[x for x in classifiers.split("\n") if x],
      data_files=data_files,
      package_dir={'psycopg2': 'lib', 'psycopg2.tests': 'tests'},
      packages=['psycopg2', 'psycopg2.tests'],
      cmdclass={
          'build_ext': psycopg_build_ext,
          'build_py': build_py, },
      ext_modules=ext)
