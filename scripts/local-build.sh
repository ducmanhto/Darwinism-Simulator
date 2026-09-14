#!/usr/bin/env bash
set -euo pipefail
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
./build/debug/darwinsim-cli config/default.yaml 250
