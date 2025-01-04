#!/usr/bin/env python
"""
We use vcpkg in github actions to build psycopg-binary.

This is a stub to work as `pg_config --libdir` or `pg_config --includedir` to
make it work with vcpkg.

You will need install `vcpkg`, set `VCPKG_ROOT` env, and run `vcpkg install
libpq:x64-windows-release` before using this script.
"""

import os
import sys
import platform
from pathlib import Path
from argparse import ArgumentParser, Namespace, RawDescriptionHelpFormatter


class ScriptError(Exception):
    """Controlled exception raised by the script."""


def _main() -> None:
    # only x64-windows
    if not (sys.platform == "win32" and platform.machine() == "AMD64"):
        raise ScriptError("this script should only be used in x64-windows")

    vcpkg_root = os.environ.get(
        "VCPKG_ROOT", os.environ.get("VCPKG_INSTALLATION_ROOT", "")
    )
    if not vcpkg_root:
        raise ScriptError("VCPKG_ROOT/VCPKG_INSTALLATION_ROOT env var not specified")
    vcpkg_platform_root = (Path(vcpkg_root) / "installed/x64-windows-release").resolve()

    args = parse_cmdline()

    if args.libdir:
        if not (f := vcpkg_platform_root / "lib/libpq.lib").exists():
            raise ScriptError(f"libpq library not found: {f}")
        print(vcpkg_platform_root.joinpath("lib"))

    elif args.includedir:
        if not (d := vcpkg_platform_root / "include/libpq").is_dir():
            raise ScriptError(f"libpq include directory not found: {d}")
        print(vcpkg_platform_root.joinpath("include"))

    else:
        raise ScriptError("command not handled")


def parse_cmdline() -> Namespace:
    parser = ArgumentParser(
        description=__doc__, formatter_class=RawDescriptionHelpFormatter
    )
    g = parser.add_mutually_exclusive_group(required=True)
    g.add_argument(
        "--libdir",
        action="store_true",
        help="show location of object code libraries",
    )
    g.add_argument(
        "--includedir",
        action="store_true",
        help="show location of C header files of the client interfaces",
    )
    opt = parser.parse_args()
    return opt


def main() -> None:
    try:
        _main()
    except ScriptError as e:
        print(f"ERROR: {e}.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
