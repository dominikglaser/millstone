#!/usr/bin/env bash
# Canonical build through the Open MPC port kit (Docker): vst/build.sh [armv7|aarch64|all]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
MV=${MPC_VST:-$ROOT/../mpc-vst-plugins}
exec "$MV/tools/build_port.sh" "$ROOT/vst/vst.json" "${1:-all}"
