#!/bin/bash

# Implement the following commands:
#
# ensure:
#
#   Get data about currently provisioned M1 server on Scaleway. If needed,
#   provision one.
#
#   The script requires the SCW_SECRET_KEY env var set to a valid secret.
#
#   If successful, return the response data on stdout. It may look like:
#
#    {
#      "id": "8b196119-3cea-4a9d-b916-265037a85e60",
#      "type": "M1-M",
#      "name": "mac-m1-psycopg",
#      "project_id": "4cf7a85e-f21e-40d4-b758-21d1f4ad3dfb",
#      "organization_id": "4cf7a85e-f21e-40d4-b758-21d1f4ad3dfb",
#      "ip": "1.2.3.4",
#      "vnc_url": "vnc://m1:PASSWORD@1.2.3.4:5900",
#      "status": "starting",
#      "created_at": "2023-09-22T18:00:18.754646Z",
#      "updated_at": "2023-09-22T18:00:18.754646Z",
#      "deletable_at": "2023-09-23T18:00:18.754646Z",
#      "zone": "fr-par-3"
#    }
#
# delete:
#
#   Delete one provisioned server, if available.
# 
# See https://www.scaleway.com/en/developers/api/apple-silicon/ for api docs.

set -euo pipefail
# set -x

project_id="4cf7a85e-f21e-40d4-b758-21d1f4ad3dfb"
zone=fr-par-3
servers_url="https://api.scaleway.com/apple-silicon/v1alpha1/zones/${zone}/servers"

function log {
    echo "$@" >&2
}
function error {
    log "ERROR: $@"
    exit 1
}

function req {
    method=$1
    shift
    curl -sSL --fail-with-body -X $method \
        -H "Content-Type: application/json" \
        -H "X-Auth-Token: ${SCW_SECRET_KEY}" \
        "$@"
}
function get {
    req GET "$@"
}
function post {
    req POST "$@"
}
function delete {
    req DELETE "$@"
}

function server_id {
    # Return the id of the first server available, else the empty string
    servers=$(get $servers_url || error "failed to request servers list")
    server_ids=$(echo "$servers" | jq -r ".servers[].id")
    for id in $server_ids; do
        echo $id
        break
    done
}

function maybe_jq {
    # Process the output via jq if displaying on console, otherwise leave
    # it unprocessed.
    if [ -t 1 ]; then
        jq .
    else
        cat
    fi
}

cmd=${1:-list}
case $cmd in
    ensure)
        id=$(server_id)
        if [[ "$id" ]]; then
            log "You have servers."
            get "$servers_url/$id" | maybe_jq
        else
            log "Creating new server."
            post $servers_url -d "
            {
                \"name\": \"mac-m1-psycopg\",
                \"project_id\": \"$project_id\",
                \"type\": \"M1-M\"
            }" | maybe_jq
        fi
        ;;
    delete)
        id=$(server_id)
        if [[ "$id" ]]; then
            log "Deleting server $id."
            delete "$servers_url/$id" | maybe_jq
        else
            log "No server found."
        fi
        ;;
    list)
        get $servers_url | maybe_jq
        ;;
    *)
        error "Usage: $(basename $0) [list|ensure|delete]"
esac
