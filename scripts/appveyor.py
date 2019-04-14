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
from urllib.request import urlopen
from tempfile import NamedTemporaryFile
from functools import lru_cache

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
    python_info()

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
    out = call_command([py_exe(), '--version'], stderr=sp.STDOUT)
    logger.info("%s", out)

    cmdline = [
        py_exe(),
        '-c',
        "import sys; print('64bit: %s' % (sys.maxsize > 2**32))",
    ]
    out = call_command(cmdline)
    logger.info("%s", out)


def step_init():
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
        out = call_command(fn)
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


def call_command(cmdline, **kwargs):
    logger.debug("calling command: %s", cmdline)
    data = sp.check_output(cmdline, **kwargs)
    return data


def setenv(k, v):
    logger.info("setting %s=%s", k, v)
    os.environ[k] = v


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
