#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p .test-build
CC=${CC:-clang}
PYTHON_BIN=${PYTHON_BIN:-python3}
$CC -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -shared -fPIC -I baseline tests/bridge.c baseline/bubbler_dsp.c baseline/bubbler_params.c -o .test-build/baseline.dylib
$CC -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -shared -fPIC -I plugin tests/bridge.c plugin/bubbler_dsp.c plugin/bubbler_params.c -o .test-build/current.dylib
"$PYTHON_BIN" tests/compare_audio.py
if [ "${1:-}" = "--benchmark" ]; then
    "$PYTHON_BIN" tests/benchmark_audio.py
fi
