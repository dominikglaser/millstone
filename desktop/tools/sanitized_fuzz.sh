#!/usr/bin/env bash
# Builds the desktop engine with AddressSanitizer and UBSan, runs the engine fuzzer on it. Needs a configured desktop/build.
# Usage: desktop/tools/sanitized_fuzz.sh [patches]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${OUT:-$ROOT/desktop/build/sanitized}
KIT=$ROOT/desktop/build/_deps/mpcvst-src/wrapper
mkdir -p "$OUT"
cd "$ROOT"
python3 - "$OUT" "$KIT" <<'PY'
import glob, json, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
out, kit = sys.argv[1:3]
src = json.load(open('vst/vst.json'))['build']['sources'] + ['src/third_party/eurorack/tides2/poly_slope_generator.cc', 'src/third_party/eurorack/tides2/resources.cc'] + glob.glob('src/third_party/eurorack/elements/dsp/*.cc') + ['src/third_party/eurorack/elements/resources.cc', 'desktop/tools/engine_fuzz.cpp']
flags = ['clang++', '-c', '-std=gnu++14', '-O1', '-g', '-w', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-DTEST', '-DMILLSTONE_DESKTOP', '-include', 'cstdio', '-Isrc', '-Isrc/third_party/eurorack', '-Ivst/build', '-I' + kit]
def cc(f):
    o = out + '/' + f.replace('/', '_') + '.o'
    r = subprocess.run(flags + [f, '-o', o], capture_output=True, text=True)
    if r.returncode: sys.exit(f + '\n' + r.stderr[:800])
    return o
objs = list(ThreadPoolExecutor(8).map(cc, src))
subprocess.check_call(['clang++', '-fsanitize=address,undefined'] + objs + ['-o', out + '/engine_fuzz'])
PY
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 ASAN_OPTIONS=detect_leaks=0 "$OUT/engine_fuzz" "${1:-40}"
