#!/usr/bin/env bash
set -euo pipefail
k3d cluster delete "${1:-darwinsim}"
