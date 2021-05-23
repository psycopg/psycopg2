#!/usr/bin/env python
"""Download packages from github actions artifacts
"""

import io
import os
import sys
import logging
from pathlib import Path
from zipfile import ZipFile

import requests

logger = logging.getLogger()
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s"
)

REPOS = "psycopg/psycopg2"
WORKFLOW_NAME = "Build packages"


class ScriptError(Exception):
    """Controlled exception raised by the script."""


def main():
    try:
        token = os.environ["GITHUB_TOKEN"]
    except KeyError:
        raise ScriptError("please set a GITHUB_TOKEN to download artifacts")

    s = requests.Session()
    s.headers["Accept"] = "application/vnd.github.v3+json"
    s.headers["Authorization"] = f"token {token}"

    logger.info("looking for recent runs")
    resp = s.get(f"https://api.github.com/repos/{REPOS}/actions/runs?per_page=10")
    resp.raise_for_status()
    for run in resp.json()["workflow_runs"]:
        if run["name"] == WORKFLOW_NAME:
            break
    else:
        raise ScriptError(f"couldn't find {WORKFLOW_NAME!r} in recent runs")

    logger.info(f"looking for run {run['id']} artifacts")
    resp = s.get(f"{run['url']}/artifacts")
    resp.raise_for_status()
    artifacts = resp.json()["artifacts"]

    dest = Path("packages")
    if not dest.exists():
        logger.info(f"creating dir {dest}")
        dest.mkdir()

    for artifact in artifacts:
        logger.info(f"downloading {artifact['name']} archive")
        zip_url = artifact["archive_download_url"]
        resp = s.get(zip_url)
        with ZipFile(io.BytesIO(resp.content)) as zf:
            logger.info("extracting archive content")
            zf.extractall(dest)

    logger.info(f"now you can run: 'twine upload -s {dest}/*'")


if __name__ == "__main__":
    try:
        sys.exit(main())

    except ScriptError as e:
        logger.error("%s", e)
        sys.exit(1)

    except KeyboardInterrupt:
        logger.info("user interrupt")
        sys.exit(1)
