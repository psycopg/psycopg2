# setup.py - distutils packaging
#
# Copyright (C) 2003-2019 Federico Di Gregorio  <fog@debian.org>
# Copyright (C) 2020-2021 The Psycopg Team
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
UPDATEs. psycopg2 also provide full asynchronous operations and support
for coroutine libraries.
"""


import os
import sys
import subprocess
from setuptools import setup, Extension
from distutils.command.build_ext import build_ext
from distutils.ccompiler import get_default_compiler
from distutils.errors import CompileError

import configparser

# Take a look at https://www.python.org/dev/peps/pep-0440/
# for a consistent versioning pattern.

PSYCOPG_VERSION = '2.9.11'


# note: if you are changing the list of supported Python version please fix
# the docs in install.rst and the /features/ page on the website.
classifiers = """\
Development Status :: 5 - Production/Stable
Intended Audience :: Developers
License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)
Programming Language :: Python
Programming Language :: Python :: 3
Programming Language :: Python :: 3.9
Programming Language :: Python :: 3.10
Programming Language :: Python :: 3.11
Programming Language :: Python :: 3.12
Programming Language :: Python :: 3.13
Programming Language :: Python :: 3.14
Programming Language :: Python :: 3 :: Only
Programming Language :: Python :: Implementation :: CPython
Programming Language :: C
Programming Language :: SQL
Topic :: Database
Topic :: Database :: Front-Ends
Topic :: Software Development
Topic :: Software Development :: Libraries :: Python Modules
Operating System :: Microsoft :: Windows
Operating System :: Unix
"""

version_flags = ['dt', 'dec']

PLATFORM_IS_WINDOWS = sys.platform.lower().startswith('win')


class PostgresConfig:
    def __init__(self, build_ext):
        self.build_ext = build_ext
        self.pg_config_exe = self.build_ext.pg_config
        if not self.pg_config_exe:
            self.pg_config_exe = self.autodetect_pg_config_path()
        if self.pg_config_exe is None:
            sys.stderr.write("""
Error: pg_config executable not found.

pg_config is required to build psycopg2 from source.  Please add the directory
containing pg_config to the $PATH or specify the full executable path with the
option:

    python setup.py build_ext --pg-config /path/to/pg_config build ...

or with the pg_config option in 'setup.cfg'.

If you prefer to avoid building psycopg2 from source, please install the PyPI
'psycopg2-binary' package instead.

For further information please check the 'doc/src/install.rst' file (also at
<https://www.psycopg.org/docs/install.html>).

""")
            sys.exit(1)

    def query(self, attr_name, *, empty_ok=False):
        """Spawn the pg_config executable, querying for the given config
        name, and return the printed value, sanitized. """
        try:
            pg_config_process = subprocess.run(
                [self.pg_config_exe, "--" + attr_name],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE)
        except OSError:
            raise Warning(
                f"Unable to find 'pg_config' file in '{self.pg_config_exe}'")
        if pg_config_process.returncode:
            err = pg_config_process.stderr.decode(errors='backslashreplace')
            raise Warning(f"pg_config --{attr_name} failed: {err}")
        result = pg_config_process.stdout.decode().strip()
        if not result and not empty_ok:
            raise Warning(f"pg_config --{attr_name} is empty")
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
        import winreg

        reg = winreg.ConnectRegistry(None, winreg.HKEY_LOCAL_MACHINE)
        try:
            pg_inst_list_key = winreg.OpenKey(reg,
                'SOFTWARE\\PostgreSQL\\Installations')
        except OSError:
            # No PostgreSQL installation, as best as we can tell.
            return None

        try:
            # Determine the name of the first subkey, if any:
            try:
                first_sub_key_name = winreg.EnumKey(pg_inst_list_key, 0)
            except OSError:
                return None

            pg_first_inst_key = winreg.OpenKey(reg,
                'SOFTWARE\\PostgreSQL\\Installations\\' + first_sub_key_name)
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

        return pg_config_path


class psycopg_build_ext(build_ext):
    """Conditionally complement the setup.cfg options file.

    This class configures the include_dirs, library_dirs, libraries
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
        self.pgdir = None
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

    built_files = 0

    def build_extension(self, extension):
        # Count files compiled to print the binary blurb only if the first fails
        compile_orig = getattr(self.compiler, '_compile', None)
        if compile_orig is not None:
            def _compile(*args, **kwargs):
                rv = compile_orig(*args, **kwargs)
                psycopg_build_ext.built_files += 1
                return rv

            self.compiler._compile = _compile

        try:
            build_ext.build_extension(self, extension)
            psycopg_build_ext.built_files += 1
        except CompileError:
            if self.built_files == 0:
                sys.stderr.write("""
It appears you are missing some prerequisite to build the package from source.

You may install a binary package by installing 'psycopg2-binary' from PyPI.
If you want to install psycopg2 from source, please install the packages
required for the build and try again.

For further information please check the 'doc/src/install.rst' file (also at
<https://www.psycopg.org/docs/install.html>).

""")
            raise

    def finalize_win32(self):
        """Finalize build system configuration on win32 platform."""

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

        for extension in ext:  # ext is a global list of Extension objects
            extension.extra_compile_args.extend(extra_compiler_args)
        # End of add-compiler-specific arguments section.

        self.libraries.append("ws2_32")
        self.libraries.append("advapi32")
        if self.compiler_is_msvc():
            # MSVC requires an explicit "libpq"
            if "pq" in self.libraries:
                self.libraries.remove("pq")
            self.libraries.append("secur32")
            self.libraries.append("libpq")
            self.libraries.append("shfolder")
            for path in self.library_dirs:
                if os.path.isfile(os.path.join(path, "ms", "libpq.lib")):
                    self.library_dirs.append(os.path.join(path, "ms"))
                    break
            if self.have_ssl:
                self.libraries.append("libcrypto")
                self.libraries.append("libssl")
                self.libraries.append("crypt32")
                self.libraries.append("user32")
                self.libraries.append("gdi32")

    def finalize_darwin(self):
        """Finalize build system configuration on darwin platform."""
        self.libraries.append('ssl')
        self.libraries.append('crypto')

    def finalize_linux(self):
        """Finalize build system configuration on GNU/Linux platform."""
        # tell piro that GCC is fine and dandy, but not so MS compilers
        for extension in self.extensions:
            extension.extra_compile_args.append(
                '-Wdeclaration-after-statement')

    finalize_linux2 = finalize_linux
    finalize_linux3 = finalize_linux

    def finalize_options(self):
        """Complete the build system configuration."""
        # An empty option in the setup.cfg causes self.libraries to include
        # an empty string in the list of libraries
        if self.libraries is not None and not self.libraries.strip():
            self.libraries = None

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

            # if present, add includedirs from cppflags, libdirs from ldflags
            tokens = pg_config_helper.query("ldflags", empty_ok=True).split()
            for token in tokens:
                if token.startswith("-L"):
                    self.library_dirs.append(token[2:])

            tokens = pg_config_helper.query("cppflags", empty_ok=True).split()
            for token in tokens:
                if token.startswith("-I"):
                    self.include_dirs.append(token[2:])

            # enable lo64 if Python 64 bits
            if is_py_64():
                define_macros.append(("HAVE_LO64", "1"))

                # Inject the flag in the version string already packed up
                # because we didn't know the version before.
                # With distutils everything is complicated.
                for i, t in enumerate(define_macros):
                    if t[0] == 'PSYCOPG_VERSION':
                        n = t[1].find(')')
                        if n > 0:
                            define_macros[i] = (
                                t[0], t[1][:n] + ' lo64' + t[1][n:])

        except Warning:
            w = sys.exc_info()[1]  # work around py 2/3 different syntax
            sys.stderr.write(f"Error: {w}\n")
            sys.exit(1)

        if hasattr(self, "finalize_" + sys.platform):
            getattr(self, "finalize_" + sys.platform)()


def is_py_64():
    # sys.maxint not available since Py 3.1;
    # sys.maxsize not available before Py 2.6;
    # this is portable at least between Py 2.4 and 3.4.
    import struct
    return struct.calcsize("P") > 4


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
    'libpq_support.c', 'win32_support.c', 'solaris_support.c', 'aix_support.c',

    'connection_int.c', 'connection_type.c',
    'cursor_int.c', 'cursor_type.c', 'column_type.c',
    'replication_connection_type.c',
    'replication_cursor_type.c',
    'replication_message_type.c',
    'diagnostics_type.c', 'error_type.c', 'conninfo_type.c',
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
    'config.h', 'pgtypes.h', 'psycopg.h', 'python.h', 'connection.h',
    'cursor.h', 'diagnostics.h', 'error.h', 'green.h', 'lobject.h',
    'replication_connection.h',
    'replication_cursor.h',
    'replication_message.h',
    'notify.h', 'pqpath.h', 'xid.h', 'column.h', 'conninfo.h',
    'libpq_support.h', 'win32_support.h', 'utils.h',

    'adapter_asis.h', 'adapter_binary.h', 'adapter_datetime.h',
    'adapter_list.h', 'adapter_pboolean.h', 'adapter_pdecimal.h',
    'adapter_pint.h', 'adapter_pfloat.h', 'adapter_qstring.h',
    'microprotocols.h', 'microprotocols_proto.h',
    'typecast.h', 'typecast_binary.h', 'sqlstate_errors.h',

    # included sources
    'typecast_array.c', 'typecast_basic.c', 'typecast_binary.c',
    'typecast_builtins.c', 'typecast_datetime.c',
]

parser = configparser.ConfigParser()
parser.read('setup.cfg')

# generate a nice version string to avoid confusion when users report bugs
version_flags.append('pq3')     # no more a choice
version_flags.append('ext')     # no more a choice

if version_flags:
    PSYCOPG_VERSION_EX = PSYCOPG_VERSION + f" ({' '.join(version_flags)})"
else:
    PSYCOPG_VERSION_EX = PSYCOPG_VERSION

define_macros.append(('PSYCOPG_VERSION', PSYCOPG_VERSION_EX))

if parser.has_option('build_ext', 'have_ssl'):
    have_ssl = parser.getboolean('build_ext', 'have_ssl')
else:
    have_ssl = False

if parser.has_option('build_ext', 'static_libpq'):
    static_libpq = parser.getboolean('build_ext', 'static_libpq')
else:
    static_libpq = False

# And now... explicitly add the defines from the .cfg files.
# Looks like setuptools or some other cog doesn't add them to the command line
# when called e.g. with "pip -e git+url'. This results in declarations
# duplicate on the commandline, which I hope is not a problem.
for define in parser.get('build_ext', 'define').split(','):
    if define:
        define_macros.append((define, '1'))

# build the extension

sources = [os.path.join('psycopg', x) for x in sources]
depends = [os.path.join('psycopg', x) for x in depends]

ext.append(Extension("psycopg2._psycopg", sources,
                     define_macros=define_macros,
                     include_dirs=include_dirs,
                     depends=depends,
                     undef_macros=[]))

try:
    f = open("README.rst")
    readme = f.read()
    f.close()
except Exception:
    print("failed to read readme: ignoring...")
    readme = __doc__

setup(name="psycopg2",
      version=PSYCOPG_VERSION,
      author="Federico Di Gregorio",
      author_email="fog@initd.org",
      maintainer="Daniele Varrazzo",
      maintainer_email="daniele.varrazzo@gmail.com",
      url="https://psycopg.org/",
      license="LGPL with exceptions",
      platforms=["any"],
      python_requires='>=3.9',
      description=readme.split("\n")[0],
      long_description="\n".join(readme.split("\n")[2:]).lstrip(),
      classifiers=[x for x in classifiers.split("\n") if x],
      data_files=data_files,
      package_dir={'psycopg2': 'lib'},
      packages=['psycopg2'],
      cmdclass={'build_ext': psycopg_build_ext},
      ext_modules=ext,
      project_urls={
          'Homepage': 'https://psycopg.org/',
          'Changes': 'https://www.psycopg.org/docs/news.html',
          'Documentation': 'https://www.psycopg.org/docs/',
          'Code': 'https://github.com/psycopg/psycopg2',
          'Issue Tracker': 'https://github.com/psycopg/psycopg2/issues',
          'Download': 'https://pypi.org/project/psycopg2/',
      })
