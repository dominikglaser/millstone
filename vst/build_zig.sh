#!/usr/bin/env bash
# Cross-compile the plugin with Zig (no Docker needed): vst/build_zig.sh [aarch64|armv7|all]
#   aarch64 -> vst/build/aarch64/millstone.so   (Gen2: MPC Live III, One G2, Key 37/61 G2, XL; MPC OS 3.x, glibc 2.39)
#   armv7   -> vst/build/armv7/millstone.so     (Gen1: Force, Live/Live II, One, X, Key 37/61; built against glibc 2.31)
# The canonical builds are the port kit's Docker ones (tools/build_port.sh, pinned gcc images); this is the
# same sources and flags on the Zig toolchain (pip install ziglang) for machines without Docker.
# Needs MPC_VST (a checkout of open-mpc/mpc-vst-plugins) and vst/build/params.h (gen_vst.py).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
MV=${MPC_VST:-$ROOT/../mpc-vst-plugins}
ZIG=${ZIG:-"python3 -m ziglang"}
TARGET=${1:-aarch64}
cd "$ROOT"
SRC=$(python3 -c "import json;print(' '.join(json.load(open('vst/vst.json'))['build']['sources']))")
SO=$(python3 -c "import json;print(json.load(open('vst/vst.json'))['so'])")
CFLAGS="-DTEST -Isrc -Isrc/third_party/eurorack -Ivst/build -I$MV/wrapper -O3 -ffast-math -fPIC -fvisibility=hidden -w"

build() {   # build <triple> <outdir> <extra flags>
  local T=$1 OUT=$2 XF=$3
  mkdir -p "$OUT"
  local OBJS=""
  for f in $SRC; do
    local o="$OUT/${f//\//_}.o"
    $ZIG c++ -target "$T" $XF $CFLAGS -include cstdio -std=gnu++14 -c "$f" -o "$o"
    OBJS="$OBJS $o"
  done
  $ZIG cc -target "$T" $XF $CFLAGS -std=gnu11 -c "$MV/wrapper/vst2_wrap.c" -o "$OUT/vst2_wrap.o"
  $ZIG c++ -target "$T" $XF -O3 -shared -fPIC -fvisibility=hidden $OBJS "$OUT/vst2_wrap.o" -lm -lpthread \
      -Wl,--no-undefined -Wl,-s -o "$OUT/$SO"
  echo "built $OUT/$SO"
  file "$OUT/$SO" | sed 's/, BuildID.*//'
  echo "exported: $(readelf --dyn-syms -W "$OUT/$SO" | grep -E ' GLOBAL .* [0-9]+ [A-Za-z]' | grep -v UND | awk '{print $8}' | tr '\n' ' ')"
  echo "highest glibc: $(readelf -V "$OUT/$SO" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)"
  sha256sum "$OUT/$SO"
}

case "$TARGET" in
  aarch64) build aarch64-linux-gnu.2.36 vst/build/aarch64 "-mcpu=cortex_a55" ;;
  armv7)   build arm-linux-gnueabihf.2.31 vst/build/armv7 "-mcpu=cortex_a17 -mfpu=neon-vfpv4 -mfloat-abi=hard" ;;
  all)     "$0" aarch64; "$0" armv7 ;;
  *) echo "usage: $0 [aarch64|armv7|all]" >&2; exit 2 ;;
esac
