#!/bin/bash

# Build psycopg2-binary wheel packages for Apple M1 (cpNNN-macosx_arm64)
#
# This script is designed to run on a local machine: it will clone the repos
# remotely and execute the `build_macos_arm64.sh` script remotely, then will
# download the built packages. A tag to build must be specified.

# The script requires a Scaleway secret key in the SCW_SECRET_KEY env var:
# It will use scaleway_m1.sh to provision a server and use it.

set -euo pipefail
# set -x

function log {
    echo "$@" >&2
}
function error {
    # Print an error message and exit.
    log "ERROR: $@"
    exit 1
}

tag=${1:-}

if [[ ! "${tag}" ]]; then
    error "Usage: $0 REF"
fi

dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

server=$("${dir}/scaleway_m1.sh" ensure)

status=$(echo "$server" | jq -r .status)
if [[ "$status" != "ready" ]]; then
    error "server status is $status"
fi

# Get user, password, ip from vnc url
tmp=$(echo "$server" | jq -r .vnc_url)  # vnc://m1:PASS@1.2.3.4:5900
tmp=${tmp/vnc:\/\//}  # m1:PASS@1.2.3.4:5900
user=${tmp%%:*}  # m1
tmp=${tmp#*:}  # PASS@1.2.3.4:5900
password=${tmp%%@*}  # PASS
tmp=${tmp#*@}  # 1.2.3.4:5900
host=${tmp%%:*}  # 1.2.3.4

ssh="ssh ${user}@${host} -o StrictHostKeyChecking=no"

# Allow the user to sudo without asking for password.
echo "$password" | \
    $ssh sh -c "test -f /etc/sudoers.d/${user} \
    || sudo -S --prompt= sh -c \
        'echo \"${user} ALL=(ALL) NOPASSWD:ALL\" > /etc/sudoers.d/${user}'"

# Clone the repos
rdir=psycobuild
$ssh rm -rf "${rdir}"
$ssh git clone https://github.com/psycopg/psycopg2.git --branch ${tag} "${rdir}"

# Build the wheel packages
$ssh "${rdir}/scripts/build/build_macos_arm64.sh"

# Transfer the packages locally
scp -r "${user}@${host}:${rdir}/wheelhouse" .
