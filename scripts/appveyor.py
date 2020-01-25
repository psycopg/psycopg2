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
    setenv('VS_VER', opt.vs_ver)

    if opt.vs_ver == '10.0' and opt.arch_64:
        setenv('DISTUTILS_USE_SDK', '1')

    path = [
        str(opt.py_dir),
        str(opt.py_dir / 'Scripts'),
        r'C:\Program Files\Git\mingw64\bin',
        os.environ['PATH'],
    ]
    setenv('PATH', os.pathsep.join(path))

    if opt.vs_ver == '9.0':
        logger.info("Fixing VS2008 Express and 64bit builds")
        shutil.copyfile(
            opt.vc_dir / "bin/vcvars64.bat",
            opt.vc_dir / "bin/amd64/vcvarsamd64.bat",
        )

    # Fix problem with VS2010 Express 64bit missing vcvars64.bat
    if opt.vs_ver == '10.0':
        if not (opt.vc_dir / "bin/amd64/vcvars64.bat").exists():
            logger.info("Fixing VS2010 Express and 64bit builds")
            copy_file(
                opt.package_dir / "scripts/vcvars64-vs2010.bat",
                opt.vc_dir / "bin/amd64/vcvars64.bat",
            )

    logger.info("Configuring compiler")
    bat_call([opt.vc_dir / "vcvarsall.bat", 'x86' if opt.arch_32 else 'amd64'])


def python_info():
    logger.info("Python Information")
    run_python(['--version'], stderr=sp.STDOUT)
    run_python(
        ['-c', "import sys; print('64bit: %s' % (sys.maxsize > 2**32))"]
    )


def step_install():
    python_info()
    configure_sdk()
    configure_postgres()

    if opt.is_wheel:
        install_wheel_support()


def install_wheel_support():
    """
    Install an up-to-date pip wheel package to build wheels.
    """
    run_python("-m pip install --upgrade pip".split())
    run_python("-m pip install wheel".split())


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
    with (opt.pg_data_dir / 'postgresql.conf').open('a') as f:
        # allow > 1 prepared transactions for test cases
        print("max_prepared_transactions = 10", file=f)
        print("ssl = on", file=f)

    # Create openssl certificate to allow ssl connection
    cwd = os.getcwd()
    os.chdir(opt.pg_data_dir)
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

    if opt.is_wheel:
        build_binary_packages()


def build_openssl():
    top = opt.ssl_build_dir
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
    zipfile = opt.cache_dir / zipname
    if not zipfile.exists():
        download(
            f"https://github.com/openssl/openssl/archive/{zipname}", zipfile
        )

    with ZipFile(zipfile) as z:
        z.extractall(path=opt.build_dir)

    sslbuild = opt.build_dir / f"openssl-OpenSSL_{ver}"
    os.chdir(sslbuild)
    run_command(
        ['perl', 'Configure', target, 'no-asm']
        + ['no-shared', 'no-zlib', f'--prefix={top}', f'--openssldir={top}']
    )

    run_command("nmake build_libs install_dev".split())

    assert (top / 'lib' / 'libssl.lib').exists()

    os.chdir(opt.clone_dir)
    shutil.rmtree(sslbuild)


def build_libpq():
    top = opt.pg_build_dir
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
    zipfile = opt.cache_dir / zipname
    if not zipfile.exists():
        download(
            f"https://github.com/postgres/postgres/archive/REL_{ver}.zip",
            zipfile,
        )

    with ZipFile(zipfile) as z:
        z.extractall(path=opt.build_dir)

    pgbuild = opt.build_dir / f"postgres-REL_{ver}"
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
            % str(opt.ssl_build_dir).replace('\\', '\\\\'),
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

    os.chdir(opt.clone_dir)
    shutil.rmtree(pgbuild)


def build_psycopg():
    os.chdir(opt.package_dir)
    patch_package_name()
    add_pg_config_path()
    run_python(
        ["setup.py", "build_ext", "--have-ssl"]
        + ["-l", "libpgcommon", "-l", "libpgport"]
        + ["-L", opt.ssl_build_dir / 'lib']
        + ['-I', opt.ssl_build_dir / 'include']
    )
    run_python(["setup.py", "build_py"])


def patch_package_name():
    """Change the psycopg2 package name in the setup.py if required."""
    if opt.package_name == 'psycopg2':
        return

    logger.info("changing package name to %s", opt.package_name)

    with (opt.package_dir / 'setup.py').open() as f:
        data = f.read()

    # Replace the name of the package with what desired
    rex = re.compile(r"""name=["']psycopg2["']""")
    assert len(rex.findall(data)) == 1, rex.findall(data)
    data = rex.sub(f'name="{opt.package_name}"', data)

    with (opt.package_dir / 'setup.py').open('w') as f:
        f.write(data)


def build_binary_packages():
    """Create wheel/exe binary packages."""
    os.chdir(opt.package_dir)

    add_pg_config_path()

    # Build .exe packages for whom still use them
    if opt.package_name == 'psycopg2':
        run_python(['setup.py', 'bdist_wininst', "-d", opt.dist_dir])

    # Build .whl packages
    run_python(['setup.py', 'bdist_wheel', "-d", opt.dist_dir])


def step_after_build():
    if not opt.is_wheel:
        install_built_package()
    else:
        install_binary_package()


def install_built_package():
    """Install the package just built by setup build."""
    os.chdir(opt.package_dir)

    # Install the psycopg just built
    add_pg_config_path()
    run_python(["setup.py", "install"])
    shutil.rmtree("psycopg2.egg-info")


def install_binary_package():
    """Install the package from a packaged wheel."""
    run_python(
        ['-m', 'pip', 'install', '--no-index', '-f', opt.dist_dir]
        + [opt.package_name]
    )


def add_pg_config_path():
    """Allow finding in the path the pg_config just built."""
    pg_path = str(opt.pg_build_dir / 'bin')
    if pg_path not in os.environ['PATH'].split(os.pathsep):
        setenv('PATH', os.pathsep.join([pg_path, os.environ['PATH']]))


def step_before_test():
    print_psycopg2_version()

    # Create and setup PostgreSQL database for the tests
    run_command([opt.pg_bin_dir / 'createdb', os.environ['PSYCOPG2_TESTDB']])
    run_command(
        [opt.pg_bin_dir / 'psql', '-d', os.environ['PSYCOPG2_TESTDB']]
        + ['-c', "CREATE EXTENSION hstore"]
    )


def print_psycopg2_version():
    """Print psycopg2 and libpq versions installed."""
    for expr in (
        'psycopg2.__version__',
        'psycopg2.__libpq_version__',
        'psycopg2.extensions.libpq_version()',
    ):
        out = out_python(['-c', f"import psycopg2; print({expr})"])
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
        out_python(
            ['-c']
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
    args = [
        '-c',
        "import tests; tests.unittest.main(defaultTest='tests.test_suite')",
    ]

    if opt.is_wheel:
        os.environ['PSYCOPG2_TEST_FAST'] = '1'
    else:
        args.append('--verbose')

    os.chdir(opt.package_dir)
    run_python(args)


def step_on_success():
    print_sha1_hashes()
    if setup_ssh():
        upload_packages()


def print_sha1_hashes():
    """
    Print the packages sha1 so their integrity can be checked upon signing.
    """
    logger.info("artifacts SHA1 hashes:")

    os.chdir(opt.package_dir / 'dist')
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
    with (opt.clone_dir / 'data/id_rsa-psycopg-upload').open('w') as f:
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

    os.chdir(opt.clone_dir)
    run_command(
        [r"C:\MinGW\msys\1.0\bin\rsync", "-avr"]
        + ["-e", r"C:\MinGW\msys\1.0\bin\ssh -F data/ssh_config"]
        + ["psycopg2/dist/", "upload:"]
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

    data = f"""\
CALL {cmdline}
{opt.py_exe} -c "import os, sys, json; \
json.dump(dict(os.environ), sys.stdout, indent=2)"
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


def ensure_dir(dir):
    if not isinstance(dir, Path):
        dir = Path(dir)

    if not dir.is_dir():
        logger.info("creating directory %s", dir)
        dir.mkdir(parents=True)

    return dir


def run_command(cmdline, **kwargs):
    """Run a command, raise on error."""
    if not isinstance(cmdline, str):
        cmdline = list(map(str, cmdline))
    logger.info("running command: %s", cmdline)
    sp.check_call(cmdline, **kwargs)


def out_command(cmdline, **kwargs):
    """Run a command, return its output, raise on error."""
    if not isinstance(cmdline, str):
        cmdline = list(map(str, cmdline))
    logger.info("running command: %s", cmdline)
    data = sp.check_output(cmdline, **kwargs)
    return data


def run_python(args, **kwargs):
    """
    Run a script in the target Python.
    """
    return run_command([opt.py_exe] + args, **kwargs)


def out_python(args, **kwargs):
    """
    Return the output of a script run in the target Python.
    """
    return out_command([opt.py_exe] + args, **kwargs)


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


class Options:
    """
    An object exposing the script configuration from env vars and command line.
    """

    @property
    def py_ver(self):
        """The Python version to build as 2 digits string."""
        rv = os.environ['PY_VER']
        assert rv in ('27', '34', '35', '36', '37', '38'), rv
        return rv

    @property
    def py_arch(self):
        """The Python architecture to build, 32 or 64."""
        rv = os.environ['PY_ARCH']
        assert rv in ('32', '64'), rv
        return int(rv)

    @property
    def arch_32(self):
        """True if the Python architecture to build is 32 bits."""
        return self.py_arch == 32

    @property
    def arch_64(self):
        """True if the Python architecture to build is 64 bits."""
        return self.py_arch == 64

    @property
    def package_name(self):
        return os.environ.get('CONFIGURATION', 'psycopg2')

    @property
    def package_version(self):
        """The psycopg2 version number to build."""
        with (self.package_dir / 'setup.py').open() as f:
            data = f.read()

        m = re.search(
            r"""^PSYCOPG_VERSION\s*=\s*['"](.*)['"]""", data, re.MULTILINE
        )
        return m.group(1)

    @property
    def is_wheel(self):
        """Are we building the wheel packages or just the extension?"""
        project_name = os.environ['APPVEYOR_PROJECT_NAME']
        if project_name == 'psycopg2':
            return False
        elif project_name == 'psycopg2-wheels':
            return True
        else:
            raise Exception(f"unexpected project name: {project_name}")

    @property
    def py_dir(self):
        """
        The path to the target python binary to execute.
        """
        dirname = ''.join(
            [r"C:\Python", self.py_ver, '-x64' if self.arch_64 else '']
        )
        return Path(dirname)

    @property
    def py_exe(self):
        """
        The full path of the target python executable.
        """
        return self.py_dir / 'python.exe'

    @property
    def vc_dir(self):
        """
        The path of the Visual C compiler.
        """
        return Path(
            r"C:\Program Files (x86)\Microsoft Visual Studio %s\VC"
            % self.vs_ver
        )

    @property
    def vs_ver(self):
        # https://wiki.python.org/moin/WindowsCompilers
        # Py 2.7 = VS Ver. 9.0 (VS 2008)
        # Py 3.3, 3.4 = VS Ver. 10.0 (VS 2010)
        # Py 3.5--3.8 = VS Ver. 14.0 (VS 2015)
        vsvers = {
            '27': '9.0',
            '33': '10.0',
            '34': '10.0',
            '35': '14.0',
            '36': '14.0',
            '37': '14.0',
            '38': '14.0',
        }
        return vsvers[self.py_ver]

    @property
    def clone_dir(self):
        """The directory where the repository is cloned."""
        return Path(r"C:\Project")

    @property
    def appveyor_pg_dir(self):
        """The directory of the postgres service made available by Appveyor."""
        return Path(os.environ['POSTGRES_DIR'])

    @property
    def pg_data_dir(self):
        """The data dir of the appveyor postgres service."""
        return self.appveyor_pg_dir / 'data'

    @property
    def pg_bin_dir(self):
        """The bin dir of the appveyor postgres service."""
        return self.appveyor_pg_dir / 'bin'

    @property
    def pg_build_dir(self):
        """The directory where to build the postgres libraries for psycopg."""
        return self.cache_arch_dir / 'postgresql'

    @property
    def ssl_build_dir(self):
        """The directory where to build the openssl libraries for psycopg."""
        return self.cache_arch_dir / 'openssl'

    @property
    def cache_arch_dir(self):
        rv = self.cache_dir / str(self.py_arch) / self.vs_ver
        return ensure_dir(rv)

    @property
    def cache_dir(self):
        return Path(r"C:\Others")

    @property
    def build_dir(self):
        rv = self.cache_arch_dir / 'Builds'
        return ensure_dir(rv)

    @property
    def package_dir(self):
        """
        The directory containing the psycopg code checkout dir.

        Building psycopg it is clone_dir, building the wheels it is a submodule.
        """
        return self.clone_dir / 'psycopg2' if self.is_wheel else self.clone_dir

    @property
    def dist_dir(self):
        """The directory where to build packages to distribute."""
        return (
            self.package_dir / 'dist' / ('psycopg2-%s' % self.package_version)
        )


def parse_cmdline():
    from argparse import ArgumentParser

    parser = ArgumentParser(description=__doc__)

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

    opt = parser.parse_args(namespace=Options())

    return opt


if __name__ == '__main__':
    sys.exit(main())
