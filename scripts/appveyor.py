#!/usr/bin/env python3
"""
Build steps for the windows binary packages.

The script is designed to be called by appveyor. Subcommands map the steps in
'appveyor.yml'.

"""

import re
import os
import sys
import json
import shutil
import logging
import subprocess as sp
from glob import glob
from zipfile import ZipFile
from tempfile import NamedTemporaryFile
from functools import lru_cache
from urllib.request import urlopen

opt = None
STEP_PREFIX = 'step_'

logger = logging.getLogger()
logging.basicConfig(
    level=logging.DEBUG, format='%(asctime)s %(levelname)s %(message)s'
)


def main():
    global opt
    opt = parse_cmdline()

    setup_env()
    cmd = globals()[STEP_PREFIX + opt.step]
    cmd()


@lru_cache()
def setup_env():
    """
    Set the environment variables according to the build environment
    """
    setenv('VS_VER', vs_ver())

    if vs_ver() == '10.0' and opt.arch_64:
        setenv('DISTUTILS_USE_SDK', '1')

    path = [
        py_dir(),
        os.path.join(py_dir(), 'Scripts'),
        r'C:\Program Files\Git\mingw64\bin',
        os.environ['PATH'],
    ]
    setenv('PATH', os.pathsep.join(path))

    if vs_ver() == '9.0':
        logger.info("Fixing VS2008 Express and 64bit builds")
        shutil.copyfile(
            os.path.join(vc_dir(), r"bin\vcvars64.bat"),
            os.path.join(vc_dir(), r"bin\amd64\vcvarsamd64.bat"),
        )

    # Fix problem with VS2010 Express 64bit missing vcvars64.bat
    # Note: repository not cloned at this point, so need to fetch
    # file another way
    if vs_ver() == '10.0':
        if not os.path.exists(
            os.path.join(vc_dir(), r"bin\amd64\vcvars64.bat")
        ):
            logger.info("Fixing VS2010 Express and 64bit builds")
            with urlopen(
                "https://raw.githubusercontent.com/psycopg/psycopg2/"
                "master/scripts/vcvars64-vs2010.bat"
            ) as f:
                data = f.read()

            with open(
                os.path.join(vc_dir(), r"bin\amd64\vcvars64.bat"), 'w'
            ) as f:
                f.write(data)

    logger.info("Configuring compiler")
    bat_call(
        [
            os.path.join(vc_dir(), "vcvarsall.bat"),
            'x86' if opt.arch_32 else 'amd64',
        ]
    )


def python_info():
    logger.info("Python Information")
    run_command([py_exe(), '--version'], stderr=sp.STDOUT)
    run_command(
        [py_exe(), '-c']
        + ["import sys; print('64bit: %s' % (sys.maxsize > 2**32))"]
    )


def step_init():
    python_info()

    # The program rc.exe on 64bit with some versions look in the wrong path
    # location when building postgresql. This cheats by copying the x64 bit
    # files to that location.
    if opt.arch_64:
        for fn in glob(
            r'C:\Program Files\Microsoft SDKs\Windows\v7.0\Bin\x64\rc*'
        ):
            logger.info("Copying %s to a better place" % os.path.basename(fn))
            shutil.copy(
                fn, r"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Bin"
            )

    # Change PostgreSQL config before service starts
    logger.info("Configuring Postgres")
    with open(os.path.join(pg_dir(), 'data', 'postgresql.conf'), 'a') as f:
        # allow > 1 prepared transactions for test cases
        print("max_prepared_transactions = 10", file=f)


def step_install():
    build_openssl()
    build_libpq()


def step_build_script():
    build_psycopg()


def build_openssl():
    top = os.path.join(base_dir(), 'openssl')
    if os.path.exists(os.path.join(top, 'lib', 'libssl.lib')):
        return

    logger.info("Building OpenSSL")

    # Setup directories for building OpenSSL libraries
    ensure_dir(os.path.join(top, 'include', 'openssl'))
    ensure_dir(os.path.join(top, 'lib'))

    # Setup OpenSSL Environment Variables based on processor architecture
    if opt.arch_32:
        target = 'VC-WIN32'
        setenv('VCVARS_PLATFORM', 'x86')
    else:
        target = 'VC-WIN64A'
        setenv('VCVARS_PLATFORM', 'amd64')
        setenv('CPU', 'AMD64')

    ver = os.environ['OPENSSL_VERSION']

    # Download OpenSSL source
    zipname = f'OpenSSL_{ver}.zip'
    zipfile = os.path.join(r'C:\Others', zipname)
    if not os.path.exists(zipfile):
        download(
            f"https://github.com/openssl/openssl/archive/{zipname}", zipfile
        )

    with ZipFile(zipfile) as z:
        z.extractall(path=build_dir())

    os.chdir(os.path.join(build_dir(), f"openssl-OpenSSL_{ver}"))
    run_command(
        ['perl', 'Configure', target, 'no-asm']
        + ['no-shared', 'no-zlib', f'--prefix={top}', f'--openssldir={top}']
    )

    run_command("nmake build_libs install_dev".split())

    assert os.path.exists(os.path.join(top, 'lib', 'libssl.lib'))

    os.chdir(base_dir())
    shutil.rmtree(os.path.join(build_dir(), f"openssl-OpenSSL_{ver}"))


def build_libpq():
    top = os.path.join(base_dir(), 'postgresql')
    if os.path.exists(os.path.join(top, 'lib', 'libpq.lib')):
        return

    logger.info("Building libpq")

    # Setup directories for building PostgreSQL librarires
    ensure_dir(os.path.join(top, 'include'))
    ensure_dir(os.path.join(top, 'lib'))
    ensure_dir(os.path.join(top, 'bin'))

    ver = os.environ['POSTGRES_VERSION']

    # Download PostgreSQL source
    zipname = f'postgres-REL_{ver}.zip'
    zipfile = os.path.join(r'C:\Others', zipname)
    if not os.path.exists(zipfile):
        download(
            f"https://github.com/postgres/postgres/archive/REL_{ver}.zip",
            zipfile,
        )

    with ZipFile(zipfile) as z:
        z.extractall(path=build_dir())

    pgbuild = os.path.join(build_dir(), f"postgres-REL_{ver}")
    os.chdir(pgbuild)

    # Patch for OpenSSL 1.1 configuration. See:
    # https://www.postgresql-archive.org/Compile-psql-9-6-with-SSL-Version-1-1-0-td6054118.html
    assert os.path.exists("src/include/pg_config.h.win32")
    with open("src/include/pg_config.h.win32", 'a') as f:
        print(
            """
#define HAVE_ASN1_STRING_GET0_DATA 1
#define HAVE_BIO_GET_DATA 1
#define HAVE_BIO_METH_NEW 1
#define HAVE_OPENSSL_INIT_SSL 1
""",
            file=f,
        )

    # Setup build config file (config.pl)
    os.chdir("src/tools/msvc")
    with open("config.pl", 'w') as f:
        print(
            """\
$config->{ldap} = 0;
$config->{openssl} = "%s";

1;
"""
            % os.path.join(base_dir(), 'openssl').replace('\\', '\\\\'),
            file=f,
        )

    # Hack the Mkvcbuild.pm file so we build the lib version of libpq
    file_replace('Mkvcbuild.pm', "'libpq', 'dll'", "'libpq', 'lib'")

    # Build libpgport, libpgcommon, libpq
    run_command([which("build"), "libpgport"])
    run_command([which("build"), "libpgcommon"])
    run_command([which("build"), "libpq"])

    # Install includes
    with open(os.path.join(pgbuild, "src/backend/parser/gram.h"), "w") as f:
        print("", file=f)

    # Copy over built libraries
    file_replace("Install.pm", "qw(Install)", "qw(Install CopyIncludeFiles)")
    run_command(
        ["perl", "-MInstall=CopyIncludeFiles", "-e"]
        + [f"chdir('../../..'); CopyIncludeFiles('{top}')"]
    )

    for lib in ('libpgport', 'libpgcommon', 'libpq'):
        shutil.copy(
            os.path.join(pgbuild, f'Release/{lib}/{lib}.lib'),
            os.path.join(top, 'lib'),
        )

    # Prepare local include directory for building from
    for dir in ('win32', 'win32_msvc'):
        merge_dir(
            os.path.join(pgbuild, f"src/include/port/{dir}"),
            os.path.join(pgbuild, "src/include"),
        )

    # Build pg_config in place
    os.chdir(os.path.join(pgbuild, 'src/bin/pg_config'))
    run_command(
        ['cl', 'pg_config.c', '/MT', '/nologo', fr'/I{pgbuild}\src\include']
        + ['/link', fr'/LIBPATH:{top}\lib']
        + ['libpgcommon.lib', 'libpgport.lib', 'advapi32.lib']
        + ['/NODEFAULTLIB:libcmt.lib']
        + [fr'/OUT:{top}\bin\pg_config.exe']
    )

    assert os.path.exists(os.path.join(top, 'lib', 'libpq.lib'))
    assert os.path.exists(os.path.join(top, 'bin', 'pg_config.exe'))

    os.chdir(base_dir())
    shutil.rmtree(os.path.join(pgbuild))


def build_psycopg():
    os.chdir(r"C:\Project")

    # Find the pg_config just built
    path = os.pathsep.join(
        [os.path.join(base_dir(), r'postgresql\bin'), os.environ['PATH']]
    )
    setenv('PATH', path)

    run_command(
        [py_exe(), "setup.py", "build_ext", "--have-ssl"]
        + ["-l", "libpgcommon", "-l", "libpgport"]
        + ["-L", os.path.join(base_dir(), r'openssl\lib')]
        + ['-I', os.path.join(base_dir(), r'openssl\include')]
    )
    run_command([py_exe(), "setup.py", "build_py"])
    run_command([py_exe(), "setup.py", "install"])
    shutil.rmtree("psycopg2.egg-info")


def step_before_test():
    # Add PostgreSQL binaries to the path
    setenv(
        'PATH',
        os.pathsep.join([os.path.join(pg_dir(), 'bin'), os.environ['PATH']]),
    )

    # Create and setup PostgreSQL database for the tests
    run_command(['createdb', os.environ['PSYCOPG2_TESTDB']])
    run_command(
        ['psql', '-d', os.environ['PSYCOPG2_TESTDB']]
        + ['-c', "CREATE EXTENSION hstore"]
    )


def step_after_build():
    # Print psycopg and libpq versions

    for expr in (
        'psycopg2.__version__',
        'psycopg2.__libpq_version__',
        'psycopg2.extensions.libpq_version()',
    ):
        out = out_command([py_exe(), '-c', f"import psycopg2; print({expr})"])
        logger.info("built %s: %s", expr, out.decode('ascii'))


def step_test_script():
    run_command(
        [py_exe(), '-c']
        + ["import tests; tests.unittest.main(defaultTest='tests.test_suite')"]
        + ["--verbose"]
    )


def download(url, fn):
    """Download a file locally"""
    logger.info("downloading %s", url)
    with open(fn, 'wb') as fo, urlopen(url) as fi:
        while 1:
            data = fi.read(8192)
            if not data:
                break
            fo.write(data)

    logger.info("file downloaded: %s", fn)


def file_replace(fn, s1, s2):
    """
    Replace all the occurrences of the string s1 into s2 in the file fn.
    """
    assert os.path.exists(fn)
    with open(fn, 'r+') as f:
        data = f.read()
        f.seek(0)
        f.write(data.replace(s1, s2))
        f.truncate()


def merge_dir(src, tgt):
    """
    Merge the content of the directory src into the directory tgt

    Reproduce the semantic of "XCOPY /Y /S src/* tgt"
    """
    for dp, _dns, fns in os.walk(src):
        logger.debug("dirpath %s", dp)
        if not fns:
            continue
        assert dp.startswith(src)
        subdir = dp[len(src) :].lstrip(os.sep)
        tgtdir = ensure_dir(os.path.join(tgt, subdir))
        for fn in fns:
            shutil.copy(os.path.join(dp, fn), tgtdir)


def bat_call(cmdline):
    """
    Simulate 'CALL' from a batch file

    Execute CALL *cmdline* and export the changed environment to the current
    environment.

    nana-nana-nana-nana...

    """
    if not isinstance(cmdline, str):
        cmdline = ' '.join(c if ' ' not in c else '"%s"' % c for c in cmdline)

    pyexe = py_exe()

    data = f"""\
CALL {cmdline}
{pyexe} -c "import os, sys, json; json.dump(dict(os.environ), sys.stdout, indent=2)"
"""

    logger.debug("preparing file to batcall:\n\n%s", data)

    with NamedTemporaryFile(suffix='.bat') as tmp:
        fn = tmp.name

    with open(fn, "w") as f:
        f.write(data)

    try:
        out = out_command(fn)
        # be vewwy vewwy caweful to print the env var as it might contain
        # secwet things like your pwecious pwivate key.
        # logger.debug("output of command:\n\n%s", out.decode('utf8', 'replace'))

        # The output has some useless crap on stdout, because sure, and json
        # indented so the last { on column 1 is where we have to start parsing

        m = list(re.finditer(b'^{', out, re.MULTILINE))[-1]
        out = out[m.start() :]
        env = json.loads(out)
        for k, v in env.items():
            if os.environ.get(k) != v:
                setenv(k, v)
    finally:
        os.remove(fn)


def py_dir():
    """
    Return the path to the target python binary to execute.
    """
    dirname = ''.join([r"C:\Python", opt.pyver, '-x64' if opt.arch_64 else ''])
    return dirname


def py_exe():
    """
    Return the full path of the target python executable.
    """
    return os.path.join(py_dir(), 'python.exe')


def vc_dir(vsver=None):
    """
    Return the path of the Visual C compiler.
    """
    if vsver is None:
        vsver = vs_ver()

    return r"C:\Program Files (x86)\Microsoft Visual Studio %s\VC" % vsver


def vs_ver(pyver=None):
    # Py 2.7 = VS Ver. 9.0 (VS 2008)
    # Py 3.4 = VS Ver. 10.0 (VS 2010)
    # Py 3.5, 3.6, 3.7 = VS Ver. 14.0 (VS 2015)
    if pyver is None:
        pyver = opt.pyver

    if pyver == '27':
        vsver = '9.0'
    elif pyver == '34':
        vsver = '10.0'
    elif pyver in ('35', '36', '37'):
        vsver = '14.0'
    else:
        raise Exception('unexpected python version: %r' % pyver)

    return vsver


def pg_dir():
    return r"C:\Program Files\PostgreSQL\9.6"


def base_dir():
    rv = r"C:\Others\%s\%s" % (opt.pyarch, vs_ver())
    return ensure_dir(rv)


def build_dir():
    rv = os.path.join(base_dir(), 'Builds')
    return ensure_dir(rv)


def ensure_dir(dir):
    if not os.path.exists(dir):
        logger.info("creating directory %s", dir)
        os.makedirs(dir)

    return dir


def run_command(cmdline, **kwargs):
    logger.debug("calling command: %s", cmdline)
    sp.check_call(cmdline, **kwargs)


def out_command(cmdline, **kwargs):
    logger.debug("calling command: %s", cmdline)
    data = sp.check_output(cmdline, **kwargs)
    return data


def setenv(k, v):
    logger.debug("setting %s=%s", k, v)
    os.environ[k] = v


def which(name):
    """
    Return the full path of a command found on the path
    """
    base, ext = os.path.splitext(name)
    if not ext:
        exts = ('.com', '.exe', '.bat', '.cmd')
    else:
        exts = (ext,)

    for dir in ['.'] + os.environ['PATH'].split(os.pathsep):
        for ext in exts:
            fn = os.path.join(dir, base + ext)
            if os.path.isfile(fn):
                return fn

    raise Exception("couldn't find program on path: %s" % name)


def parse_cmdline():
    from argparse import ArgumentParser

    parser = ArgumentParser(description=__doc__)

    parser.add_argument(
        '--pyver',
        choices='27 34 35 36 37'.split(),
        help="the target python version. Default from PYVER env var",
    )

    parser.add_argument(
        '--pyarch',
        choices='32 64'.split(),
        help="the target python architecture. Default from PYTHON_ARCH env var",
    )

    steps = [
        n[len(STEP_PREFIX) :]
        for n in globals()
        if n.startswith(STEP_PREFIX) and callable(globals()[n])
    ]

    parser.add_argument(
        'step', choices=steps, help="the appveyor step to execute"
    )

    opt = parser.parse_args()

    # And die if they are not there.
    if not opt.pyver:
        opt.pyver = os.environ['PYVER']
    if not opt.pyarch:
        opt.pyarch = os.environ['PYTHON_ARCH']
        assert opt.pyarch in ('32', '64')

    opt.arch_32 = opt.pyarch == '32'
    opt.arch_64 = opt.pyarch == '64'

    return opt


if __name__ == '__main__':
    sys.exit(main())
