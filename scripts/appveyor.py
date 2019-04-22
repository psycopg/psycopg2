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
from pathlib import Path
from zipfile import ZipFile
from tempfile import NamedTemporaryFile
from urllib.request import urlopen

opt = None
STEP_PREFIX = 'step_'

logger = logging.getLogger()
logging.basicConfig(
    level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s'
)


def main():
    global opt
    opt = parse_cmdline()
    logger.setLevel(opt.loglevel)

    cmd = globals()[STEP_PREFIX + opt.step]
    cmd()


def setup_build_env():
    """
    Set the environment variables according to the build environment
    """
    setenv('VS_VER', vs_ver())

    if vs_ver() == '10.0' and opt.arch_64:
        setenv('DISTUTILS_USE_SDK', '1')

    path = [
        str(py_dir()),
        str(py_dir() / 'Scripts'),
        r'C:\Program Files\Git\mingw64\bin',
        os.environ['PATH'],
    ]
    setenv('PATH', os.pathsep.join(path))

    if vs_ver() == '9.0':
        logger.info("Fixing VS2008 Express and 64bit builds")
        shutil.copyfile(
            vc_dir() / r"bin\vcvars64.bat",
            vc_dir() / r"bin\amd64\vcvarsamd64.bat",
        )

    # Fix problem with VS2010 Express 64bit missing vcvars64.bat
    if vs_ver() == '10.0':
        if not (vc_dir() / r"bin\amd64\vcvars64.bat").exists():
            logger.info("Fixing VS2010 Express and 64bit builds")
            copy_file(
                package_dir() / r"scripts\vcvars64-vs2010.bat",
                vc_dir() / r"bin\amd64\vcvars64.bat",
            )

    logger.info("Configuring compiler")
    bat_call([vc_dir() / "vcvarsall.bat", 'x86' if opt.arch_32 else 'amd64'])


def python_info():
    logger.info("Python Information")
    run_command([py_exe(), '--version'], stderr=sp.STDOUT)
    run_command(
        [py_exe(), '-c']
        + ["import sys; print('64bit: %s' % (sys.maxsize > 2**32))"]
    )


def step_install():
    python_info()
    configure_sdk()
    configure_postgres()

    if is_wheel():
        install_wheel_support()


def install_wheel_support():
    """
    Install an up-to-date pip wheel package to build wheels.
    """
    run_command([py_exe()] + "-m pip install --upgrade pip".split())
    run_command([py_exe()] + "-m pip install wheel".split())


def configure_sdk():
    # The program rc.exe on 64bit with some versions look in the wrong path
    # location when building postgresql. This cheats by copying the x64 bit
    # files to that location.
    if opt.arch_64:
        for fn in glob(
            r'C:\Program Files\Microsoft SDKs\Windows\v7.0\Bin\x64\rc*'
        ):
            copy_file(
                fn, r"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Bin"
            )


def configure_postgres():
    """
    Set up PostgreSQL config before the service starts.
    """
    logger.info("Configuring Postgres")
    with (pg_data_dir() / 'postgresql.conf').open('a') as f:
        # allow > 1 prepared transactions for test cases
        print("max_prepared_transactions = 10", file=f)
        print("ssl = on", file=f)

    # Create openssl certificate to allow ssl connection
    cwd = os.getcwd()
    os.chdir(pg_data_dir())
    run_openssl(
        'req -new -x509 -days 365 -nodes -text '
        '-out server.crt -keyout server.key -subj /CN=initd.org'.split()
    )
    run_openssl(
        'req -new -nodes -text -out root.csr -keyout root.key '
        '-subj /CN=initd.org'.split()
    )

    run_openssl(
        'x509 -req -in root.csr -text -days 3650 -extensions v3_ca '
        '-signkey root.key -out root.crt'.split()
    )

    run_openssl(
        'req -new -nodes -text -out server.csr -keyout server.key '
        '-subj /CN=initd.org'.split()
    )

    run_openssl(
        'x509 -req -in server.csr -text -days 365 -CA root.crt '
        '-CAkey root.key -CAcreateserial -out server.crt'.split()
    )

    os.chdir(cwd)


def run_openssl(args):
    """Run the appveyor-installed openssl with some args."""
    # https://www.appveyor.com/docs/windows-images-software/
    openssl = Path(r"C:\OpenSSL-v111-Win64") / 'bin' / 'openssl'
    return run_command([openssl] + args)


def step_build_script():
    setup_build_env()
    build_openssl()
    build_libpq()
    build_psycopg()

    if is_wheel():
        build_binary_packages()


def build_openssl():
    top = ssl_build_dir()
    if (top / 'lib' / 'libssl.lib').exists():
        return

    logger.info("Building OpenSSL")

    # Setup directories for building OpenSSL libraries
    ensure_dir(top / 'include' / 'openssl')
    ensure_dir(top / 'lib')

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
    zipfile = cache_dir() / zipname
    if not zipfile.exists():
        download(
            f"https://github.com/openssl/openssl/archive/{zipname}", zipfile
        )

    with ZipFile(zipfile) as z:
        z.extractall(path=build_dir())

    sslbuild = build_dir() / f"openssl-OpenSSL_{ver}"
    os.chdir(sslbuild)
    run_command(
        ['perl', 'Configure', target, 'no-asm']
        + ['no-shared', 'no-zlib', f'--prefix={top}', f'--openssldir={top}']
    )

    run_command("nmake build_libs install_dev".split())

    assert (top / 'lib' / 'libssl.lib').exists()

    os.chdir(clone_dir())
    shutil.rmtree(sslbuild)


def build_libpq():
    top = pg_build_dir()
    if (top / 'lib' / 'libpq.lib').exists():
        return

    logger.info("Building libpq")

    # Setup directories for building PostgreSQL librarires
    ensure_dir(top / 'include')
    ensure_dir(top / 'lib')
    ensure_dir(top / 'bin')

    ver = os.environ['POSTGRES_VERSION']

    # Download PostgreSQL source
    zipname = f'postgres-REL_{ver}.zip'
    zipfile = cache_dir() / zipname
    if not zipfile.exists():
        download(
            f"https://github.com/postgres/postgres/archive/REL_{ver}.zip",
            zipfile,
        )

    with ZipFile(zipfile) as z:
        z.extractall(path=build_dir())

    pgbuild = build_dir() / f"postgres-REL_{ver}"
    os.chdir(pgbuild)

    # Patch for OpenSSL 1.1 configuration. See:
    # https://www.postgresql-archive.org/Compile-psql-9-6-with-SSL-Version-1-1-0-td6054118.html
    assert Path("src/include/pg_config.h.win32").exists()
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
            % str(ssl_build_dir()).replace('\\', '\\\\'),
            file=f,
        )

    # Hack the Mkvcbuild.pm file so we build the lib version of libpq
    file_replace('Mkvcbuild.pm', "'libpq', 'dll'", "'libpq', 'lib'")

    # Build libpgport, libpgcommon, libpq
    run_command([which("build"), "libpgport"])
    run_command([which("build"), "libpgcommon"])
    run_command([which("build"), "libpq"])

    # Install includes
    with (pgbuild / "src/backend/parser/gram.h").open("w") as f:
        print("", file=f)

    # Copy over built libraries
    file_replace("Install.pm", "qw(Install)", "qw(Install CopyIncludeFiles)")
    run_command(
        ["perl", "-MInstall=CopyIncludeFiles", "-e"]
        + [f"chdir('../../..'); CopyIncludeFiles('{top}')"]
    )

    for lib in ('libpgport', 'libpgcommon', 'libpq'):
        copy_file(pgbuild / f'Release/{lib}/{lib}.lib', top / 'lib')

    # Prepare local include directory for building from
    for dir in ('win32', 'win32_msvc'):
        merge_dir(pgbuild / f"src/include/port/{dir}", pgbuild / "src/include")

    # Build pg_config in place
    os.chdir(pgbuild / 'src/bin/pg_config')
    run_command(
        ['cl', 'pg_config.c', '/MT', '/nologo', fr'/I{pgbuild}\src\include']
        + ['/link', fr'/LIBPATH:{top}\lib']
        + ['libpgcommon.lib', 'libpgport.lib', 'advapi32.lib']
        + ['/NODEFAULTLIB:libcmt.lib']
        + [fr'/OUT:{top}\bin\pg_config.exe']
    )

    assert (top / 'lib' / 'libpq.lib').exists()
    assert (top / 'bin' / 'pg_config.exe').exists()

    os.chdir(clone_dir())
    shutil.rmtree(pgbuild)


def build_psycopg():
    os.chdir(package_dir())
    patch_package_name()
    add_pg_config_path()
    run_command(
        [py_exe(), "setup.py", "build_ext", "--have-ssl"]
        + ["-l", "libpgcommon", "-l", "libpgport"]
        + ["-L", ssl_build_dir() / 'lib']
        + ['-I', ssl_build_dir() / 'include']
    )
    run_command([py_exe(), "setup.py", "build_py"])


def patch_package_name():
    """Change the psycopg2 package name in the setup.py if required."""
    conf = os.environ.get('CONFIGURATION', 'psycopg2')
    if conf == 'psycopg2':
        return

    logger.info("changing package name to %s", conf)

    with (package_dir() / 'setup.py').open() as f:
        data = f.read()

    # Replace the name of the package with what desired
    rex = re.compile(r"""name=["']psycopg2["']""")
    assert len(rex.findall(data)) == 1, rex.findall(data)
    data = rex.sub(f'name="{conf}"', data)

    with (package_dir() / 'setup.py').open('w') as f:
        f.write(data)


def build_binary_packages():
    """Create wheel/exe binary packages."""
    os.chdir(package_dir())

    add_pg_config_path()

    # Build .exe packages for whom still use them
    if os.environ['CONFIGURATION'] == 'psycopg2':
        run_command([py_exe(), 'setup.py', 'bdist_wininst', "-d", dist_dir()])

    # Build .whl packages
    run_command([py_exe(), 'setup.py', 'bdist_wheel', "-d", dist_dir()])


def step_after_build():
    if not is_wheel():
        install_built_package()
    else:
        install_binary_package()


def install_built_package():
    """Install the package just built by setup build."""
    os.chdir(package_dir())

    # Install the psycopg just built
    add_pg_config_path()
    run_command([py_exe(), "setup.py", "install"])
    shutil.rmtree("psycopg2.egg-info")


def install_binary_package():
    """Install the package from a packaged wheel."""
    run_command(
        [py_exe(), '-m', 'pip', 'install', '--no-index', '-f', dist_dir()]
        + [os.environ['CONFIGURATION']]
    )


def add_pg_config_path():
    """Allow finding in the path the pg_config just built."""
    pg_path = str(pg_build_dir() / 'bin')
    if pg_path not in os.environ['PATH'].split(os.pathsep):
        setenv('PATH', os.pathsep.join([pg_path, os.environ['PATH']]))


def step_before_test():
    print_psycopg2_version()

    # Create and setup PostgreSQL database for the tests
    run_command([pg_bin_dir() / 'createdb', os.environ['PSYCOPG2_TESTDB']])
    run_command(
        [pg_bin_dir() / 'psql', '-d', os.environ['PSYCOPG2_TESTDB']]
        + ['-c', "CREATE EXTENSION hstore"]
    )


def print_psycopg2_version():
    """Print psycopg2 and libpq versions installed."""
    for expr in (
        'psycopg2.__version__',
        'psycopg2.__libpq_version__',
        'psycopg2.extensions.libpq_version()',
    ):
        out = out_command([py_exe(), '-c', f"import psycopg2; print({expr})"])
        logger.info("built %s: %s", expr, out.decode('ascii'))


def step_test_script():
    check_libpq_version()
    run_test_suite()


def check_libpq_version():
    """
    Fail if the package installed is not using the expected libpq version.
    """
    want_ver = tuple(map(int, os.environ['POSTGRES_VERSION'].split('_')))
    want_ver = "%d%04d" % want_ver
    got_ver = (
        out_command(
            [py_exe(), '-c']
            + ["import psycopg2; print(psycopg2.extensions.libpq_version())"]
        )
        .decode('ascii')
        .rstrip()
    )
    assert want_ver == got_ver, "libpq version mismatch: %r != %r" % (
        want_ver,
        got_ver,
    )


def run_test_suite():
    # Remove this var, which would make badly a configured OpenSSL 1.1 work
    os.environ.pop('OPENSSL_CONF', None)

    # Run the unit test
    os.chdir(package_dir())
    run_command(
        [py_exe(), '-c']
        + ["import tests; tests.unittest.main(defaultTest='tests.test_suite')"]
        + ["--verbose"]
    )


def step_on_success():
    print_sha1_hashes()
    if setup_ssh():
        upload_packages()


def print_sha1_hashes():
    """
    Print the packages sha1 so their integrity can be checked upon signing.
    """
    logger.info("artifacts SHA1 hashes:")

    os.chdir(package_dir() / 'dist')
    run_command([which('sha1sum'), '-b', f'psycopg2-*/*'])


def setup_ssh():
    """
    Configure ssh to upload built packages where they can be retrieved.

    Return False if can't configure and upload shoould be skipped.
    """
    # If we are not on the psycopg AppVeyor account, the environment variable
    # REMOTE_KEY will not be decrypted. In that case skip uploading.
    if os.environ['APPVEYOR_ACCOUNT_NAME'] != 'psycopg':
        logger.warn("skipping artifact upload: you are not psycopg")
        return False

    pkey = os.environ.get('REMOTE_KEY', None)
    if not pkey:
        logger.warn("skipping artifact upload: no remote key")
        return False

    # Write SSH Private Key file from environment variable
    pkey = pkey.replace(' ', '\n')
    with (clone_dir() / 'id_rsa').open('w') as f:
        f.write(
            f"""\
-----BEGIN RSA PRIVATE KEY-----
{pkey}
-----END RSA PRIVATE KEY-----
"""
        )

    # Make a directory to please MinGW's version of ssh
    ensure_dir(r"C:\MinGW\msys\1.0\home\appveyor\.ssh")

    return True


def upload_packages():
    # Upload built artifacts
    logger.info("uploading artifacts")

    ssh_cmd = r"C:\MinGW\msys\1.0\bin\ssh -i %s -o UserKnownHostsFile=%s" % (
        clone_dir() / "id_rsa",
        clone_dir() / 'known_hosts',
    )

    os.chdir(package_dir())
    run_command(
        [r"C:\MinGW\msys\1.0\bin\rsync", "-avr"]
        + ["-e", ssh_cmd, "dist/", "upload@initd.org:"]
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
    src = str(src)
    for dp, _dns, fns in os.walk(src):
        logger.debug("dirpath %s", dp)
        if not fns:
            continue
        assert dp.startswith(src)
        subdir = dp[len(src) :].lstrip(os.sep)
        tgtdir = ensure_dir(os.path.join(tgt, subdir))
        for fn in fns:
            copy_file(os.path.join(dp, fn), tgtdir)


def bat_call(cmdline):
    """
    Simulate 'CALL' from a batch file

    Execute CALL *cmdline* and export the changed environment to the current
    environment.

    nana-nana-nana-nana...

    """
    if not isinstance(cmdline, str):
        cmdline = map(str, cmdline)
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
    return Path(dirname)


def py_exe():
    """
    Return the full path of the target python executable.
    """
    return py_dir() / 'python.exe'


def vc_dir(vsver=None):
    """
    Return the path of the Visual C compiler.
    """
    if vsver is None:
        vsver = vs_ver()

    return Path(
        r"C:\Program Files (x86)\Microsoft Visual Studio %s\VC" % vsver
    )


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


def clone_dir():
    return Path(r"C:\Project")


def appveyor_pg_dir():
    return Path(os.environ['POSTGRES_DIR'])


def pg_data_dir():
    return appveyor_pg_dir() / 'data'


def pg_bin_dir():
    return appveyor_pg_dir() / 'bin'


def pg_build_dir():
    return cache_arch_dir() / 'postgresql'


def ssl_build_dir():
    return cache_arch_dir() / 'openssl'


def cache_arch_dir():
    rv = cache_dir() / opt.pyarch / vs_ver()
    return ensure_dir(rv)


def cache_dir():
    return Path(r"C:\Others")


def build_dir():
    rv = cache_arch_dir() / 'Builds'
    return ensure_dir(rv)


def package_dir():
    """
    Return the directory containing the psycopg code checkout dir

    Building psycopg is clone_dir(), building the wheel packages is a submodule.
    """
    return clone_dir() / 'psycopg2' if is_wheel() else clone_dir()


def is_wheel():
    """
    Return whether we are building the wheel packages or just the extension.
    """
    project_name = os.environ['APPVEYOR_PROJECT_NAME']
    if project_name == 'psycopg2':
        return False
    elif project_name == 'psycopg2-wheels':
        return True
    else:
        raise Exception(f"unexpected project name: {project_name}")


def dist_dir():
    return package_dir() / 'dist' / ('psycopg2-%s' % package_version())


def package_version():
    with (package_dir() / 'setup.py').open() as f:
        data = f.read()

    m = re.search(
        r"""^PSYCOPG_VERSION\s*=\s*['"](.*)['"]""", data, re.MULTILINE
    )
    return m.group(1)


def ensure_dir(dir):
    if not isinstance(dir, Path):
        dir = Path(dir)

    if not dir.is_dir():
        logger.info("creating directory %s", dir)
        dir.mkdir(parents=True)

    return dir


def run_command(cmdline, **kwargs):
    if not isinstance(cmdline, str):
        cmdline = list(map(str, cmdline))
    logger.info("running command: %s", cmdline)
    sp.check_call(cmdline, **kwargs)


def out_command(cmdline, **kwargs):
    if not isinstance(cmdline, str):
        cmdline = list(map(str, cmdline))
    logger.info("running command: %s", cmdline)
    data = sp.check_output(cmdline, **kwargs)
    return data


def copy_file(src, dst):
    logger.info("copying file %s -> %s", src, dst)
    shutil.copy(src, dst)


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

    g = parser.add_mutually_exclusive_group()
    g.add_argument(
        '-q',
        '--quiet',
        help="Talk less",
        dest='loglevel',
        action='store_const',
        const=logging.WARN,
        default=logging.INFO,
    )
    g.add_argument(
        '-v',
        '--verbose',
        help="Talk more",
        dest='loglevel',
        action='store_const',
        const=logging.DEBUG,
        default=logging.INFO,
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
