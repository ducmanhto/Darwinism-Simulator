#!/usr/bin/env bash
set -euo pipefail

NAMESPACE="${NAMESPACE:-darwinsim}"
DEPLOYMENT="${DEPLOYMENT:-darwinsim-controller}"
RESET_POD="darwinsim-reset"

if [[ "${1:-}" != "--yes" ]]; then
    echo "This will permanently delete the active DarwinSim checkpoint"
    echo "and SQLite history, then start a new simulation at tick 0."
    echo
    echo "Run:"
    echo "  $0 --yes"
    exit 1
fi

ORIGINAL_REPLICAS="$(
    kubectl -n "$NAMESPACE" get deployment "$DEPLOYMENT" \
      -o jsonpath='{.spec.replicas}'
)"

BACKEND_IMAGE="$(
    kubectl -n "$NAMESPACE" get deployment "$DEPLOYMENT" \
      -o jsonpath='{.spec.template.spec.containers[0].image}'
)"

echo "Controller replicas: $ORIGINAL_REPLICAS"
echo "Backend image: $BACKEND_IMAGE"

restore_controller() {
    echo "Restoring controller deployment..."
    kubectl -n "$NAMESPACE" scale \
      deployment/"$DEPLOYMENT" \
      --replicas="$ORIGINAL_REPLICAS" >/dev/null || true
}

trap restore_controller EXIT

echo "Stopping controller..."

kubectl -n "$NAMESPACE" scale \
  deployment/"$DEPLOYMENT" \
  --replicas=0

kubectl -n "$NAMESPACE" wait \
  --for=delete pod \
  -l app=darwinsim-controller \
  --timeout=90s || true

kubectl -n "$NAMESPACE" delete pod "$RESET_POD" \
  --ignore-not-found=true \
  --wait=true

echo "Clearing persistent simulation state..."

cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: Pod
metadata:
  name: ${RESET_POD}
  namespace: ${NAMESPACE}
spec:
  restartPolicy: Never

  containers:
  - name: reset
    image: ${BACKEND_IMAGE}
    imagePullPolicy: IfNotPresent

    command:
    - /bin/sh
    - -c
    - |
      set -eu

      echo "Before reset:"
      ls -lah /var/lib/darwinsim || true

      rm -f \
        /var/lib/darwinsim/latest.chk \
        /var/lib/darwinsim/darwinsim.sqlite \
        /var/lib/darwinsim/darwinsim.sqlite-shm \
        /var/lib/darwinsim/darwinsim.sqlite-wal

      echo
      echo "After reset:"
      ls -lah /var/lib/darwinsim || true

    volumeMounts:
    - name: data
      mountPath: /var/lib/darwinsim

  volumes:
  - name: data
    persistentVolumeClaim:
      claimName: darwinsim-data
EOF

kubectl -n "$NAMESPACE" wait \
  --for=jsonpath='{.status.phase}'=Succeeded \
  pod/"$RESET_POD" \
  --timeout=90s

kubectl -n "$NAMESPACE" logs "$RESET_POD"

kubectl -n "$NAMESPACE" delete pod "$RESET_POD" \
  --wait=true

echo "Starting controller..."

kubectl -n "$NAMESPACE" scale \
  deployment/"$DEPLOYMENT" \
  --replicas="$ORIGINAL_REPLICAS"

kubectl -n "$NAMESPACE" rollout status \
  deployment/"$DEPLOYMENT" \
  --timeout=180s

trap - EXIT

echo
echo "Fresh DarwinSim started."

if curl -fsS http://localhost:8088/api/state >/tmp/darwinsim-reset-state.json 2>/dev/null; then
    jq . /tmp/darwinsim-reset-state.json
    rm -f /tmp/darwinsim-reset-state.json
fi