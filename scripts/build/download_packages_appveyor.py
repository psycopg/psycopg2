#!/usr/bin/env python
"""Download packages from github actions artifacts
"""

import os
import sys
import logging
from pathlib import Path

import requests

logger = logging.getLogger()
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

API_URL = "https://ci.appveyor.com/api"
REPOS = "psycopg/psycopg2"
WORKFLOW_NAME = "Build packages"


class ScriptError(Exception):
    """Controlled exception raised by the script."""


def main():
    try:
        token = os.environ["APPVEYOR_TOKEN"]
    except KeyError:
        raise ScriptError("please set a APPVEYOR_TOKEN to download artifacts")

    s = requests.Session()
    s.headers["Content-Type"] = "application/json"
    s.headers["Authorization"] = f"Bearer {token}"

    logger.info("fetching last run")
    resp = s.get(f"{API_URL}/projects/{REPOS}/")
    resp.raise_for_status()
    data = resp.json()
    jobs = data["build"]["jobs"]
    for job in jobs:
        if job["status"] != "success":
            raise ScriptError("status for job {job['jobId']} is {job['status']}")

        logger.info(f"fetching artifacts info for {job['name']}")
        resp = s.get(f"{API_URL}/buildjobs/{job['jobId']}/artifacts/")
        resp.raise_for_status()
        afs = resp.json()
        for af in afs:
            fn = af["fileName"]
            if fn.startswith("dist/"):
                fn = fn.split("/", 1)[1]
            dest = Path("packages") / fn
            logger.info(f"downloading {dest}")
            resp = s.get(
                f"{API_URL}/buildjobs/{job['jobId']}/artifacts/{af['fileName']}"
            )
            resp.raise_for_status()
            if not dest.parent.exists():
                dest.parent.mkdir()

            with dest.open("wb") as f:
                f.write(resp.content)

    logger.info("now you can run: 'twine upload -s packages/*'")


if __name__ == "__main__":
    try:
        sys.exit(main())

    except ScriptError as e:
        logger.error("%s", e)
        sys.exit(1)

    except KeyboardInterrupt:
        logger.info("user interrupt")
        sys.exit(1)

