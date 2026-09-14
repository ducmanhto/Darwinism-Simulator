#!/usr/bin/env bash
set -euo pipefail

BIN=${1:-./build/release/darwinsim-cli}
CONFIG=${2:-config/default.yaml}
TICKS=${3:-1000}

echo "Benchmarking $TICKS ticks using $BIN"
/usr/bin/time -f 'elapsed=%e sec max_rss=%M KB cpu=%P' "$BIN" "$CONFIG" "$TICKS" >/tmp/darwinsim-benchmark.out
TAIL=$(tail -1 /tmp/darwinsim-benchmark.out)
echo "$TAIL"
