#!/usr/bin/env bash
# Build and run the engine tests natively (x86): tests/run.sh [asan|fast] [demo.wav]
# Needs MPC_VST (a checkout of open-mpc/mpc-vst-plugins) and vst/build/params.h (gen_vst.py --params-h).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
MV=${MPC_VST:-$ROOT/../mpc-vst-plugins}
MODE=${1:-fast}
DEMO=${2:-}
OUT=$ROOT/build/test_$MODE
mkdir -p "$OUT"
SRC=$(python3 -c "import json;print(' '.join(json.load(open('$ROOT/vst/vst.json'))['build']['sources']))")
FLAGS="-std=gnu++14 -DTEST -include cstdio -I$ROOT/src -I$ROOT/src/third_party/eurorack -I$ROOT/vst/build -I$MV/wrapper -w"
if [ "$MODE" = asan ]; then FLAGS="$FLAGS -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"; else FLAGS="$FLAGS -O2 -ffast-math"; fi
cd "$ROOT"
g++ $FLAGS $SRC tests/engine_test.cpp -lm -o "$OUT/engine_test"
"$OUT/engine_test" $DEMO
