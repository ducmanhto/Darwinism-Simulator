#!/usr/bin/env bash
set -euo pipefail

URL=${DARWINSIM_URL:-http://localhost:8088}
BEFORE=$(curl -fsS "$URL/api/state" | jq -r .tick)
echo "Tick before worker deletion: $BEFORE"
kubectl -n darwinsim delete pod darwinsim-worker-0
kubectl -n darwinsim rollout status statefulset/darwinsim-worker --timeout=180s
sleep 5
AFTER=$(curl -fsS "$URL/api/state" | jq -r .tick)
echo "Tick after worker restart: $AFTER"
if (( AFTER < BEFORE )); then
   echo "Unexpected rollback" >&2
   exit 1
fi
echo "Recovery test passed"
