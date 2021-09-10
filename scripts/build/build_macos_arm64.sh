#!/bin/bash
#
# Script to generate arm64 wheels on an Apple Silicon host
#
# TODO: File issue on why cross-compiling is not working
python3 -m venv .venv
source .venv/bin/activate
pip install -U pip wheel
pip install cibuildwheel
# You will be prompted for your password in order to install Python under /Applications
PACKAGE_NAME=psycopg2-binary
CIBW_BUILD=cp38-macosx_arm64 cibuildwheel . --output-dir wheelhouse --platform macos 2>&1
