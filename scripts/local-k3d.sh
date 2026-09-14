#!/usr/bin/env bash
set -euo pipefail

CLUSTER=${1:-darwinsim}
k3d cluster delete "$CLUSTER" >/dev/null 2>&1 || true
k3d cluster create "$CLUSTER" \
  -p "8088:80@loadbalancer" \
  --k3s-arg "--disable=traefik@server:*" \
  --wait

docker build -f docker/backend.Dockerfile -t darwinsim-backend:local .
docker build -f docker/frontend.Dockerfile -t darwinsim-frontend:local .
k3d image import darwinsim-backend:local -c "$CLUSTER"
k3d image import darwinsim-frontend:local -c "$CLUSTER"

kubectl apply -k kubernetes/local
kubectl -n darwinsim rollout status statefulset/darwinsim-worker --timeout=180s
kubectl -n darwinsim rollout status deployment/darwinsim-controller --timeout=180s
kubectl -n darwinsim rollout status deployment/darwinsim-web --timeout=180s

echo "DarwinSim local UI: http://localhost:8088"
echo "Waiting for DarwinSim API..."

STATE_FILE=$(mktemp)
API_READY=0

for i in $(seq 1 30); do
    if curl -fsS \
        http://127.0.0.1:8088/api/state \
        > "$STATE_FILE"; then

        API_READY=1
        break
    fi

    echo "API not ready yet ($i/30)..."
    sleep 2
done

if [ "$API_READY" -ne 1 ]; then
    echo "ERROR: DarwinSim API did not become reachable." >&2

    echo
    echo "=== Pods ==="
    kubectl -n darwinsim get pods -o wide || true

    echo
    echo "=== Services ==="
    kubectl -n darwinsim get services || true

    echo
    echo "=== Controller logs ==="
    kubectl -n darwinsim logs \
        deployment/darwinsim-controller \
        --tail=50 || true

    echo
    echo "=== Web logs ==="
    kubectl -n darwinsim logs \
        deployment/darwinsim-web \
        --tail=50 || true

    rm -f "$STATE_FILE"
    exit 1
fi

echo "DarwinSim API is ready:"
cat "$STATE_FILE"
echo

rm -f "$STATE_FILE"
