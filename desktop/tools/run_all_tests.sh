#!/usr/bin/env bash
# Every check in one go. Run from anywhere, after: cmake -B desktop/build -S desktop -DMILLSTONE_UI_SNAPSHOT=ON && cmake --build desktop/build
# Add --sanitize for the slow engine fuzz under AddressSanitizer and UBSan.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
B=$ROOT/desktop/build
OUT=$(mktemp -d)
FAILED=0
step() { name=$1; shift; if "$@" > "$OUT/$name.txt" 2>&1; then echo "PASS  $name: $(tail -1 "$OUT/$name.txt")"; else echo "FAIL  $name"; grep -E '^FAIL' "$OUT/$name.txt" | head -30; tail -3 "$OUT/$name.txt"; FAILED=1; fi; }
step fx "$B/millstone_fx_test"
step fxplugin "$B/millstone_fx_plugin_test"
step behaviour "$B/millstone_behaviour_test"
step presets "$B/millstone_preset_test"
step ui "$B/millstone_ui_snapshot" "$OUT"
step stress "$B/millstone_stress_test"
step engine env MPC_VST="$B/_deps/mpcvst-src" bash "$ROOT/tests/run.sh" fast
clang++ -std=gnu++14 -O2 -w -I"$ROOT/src" -I"$ROOT/vst/build" -I"$B/_deps/mpcvst-src/wrapper" "$ROOT/desktop/tools/engine_fuzz.cpp" "$B/libmillstone_engine.a" -o "$OUT/engine_fuzz"
step fuzz "$OUT/engine_fuzz" 300
clang++ -std=gnu++14 -O2 -w -I"$ROOT/src" -I"$ROOT/vst/build" -I"$B/_deps/mpcvst-src/wrapper" "$ROOT/desktop/tools/tides_vca_test.cpp" "$B/libmillstone_engine.a" -o "$OUT/tides_vca_test"
step tides "$OUT/tides_vca_test"
if [ "${1:-}" = "--sanitize" ]; then step sanitized "$ROOT/desktop/tools/sanitized_fuzz.sh" 40; fi
[ $FAILED = 0 ] && echo "ALL PASSED" || echo "SOME FAILED"
exit $FAILED
