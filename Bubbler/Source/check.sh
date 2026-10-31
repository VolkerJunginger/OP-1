#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p .test-build
CC=${CC:-clang}
$CC -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I plugin tests/test_dsp.c plugin/bubbler_dsp.c plugin/bubbler_params.c -o .test-build/test_dsp
.test-build/test_dsp
$CC -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I plugin-kit/sdk -I plugin tests/test_adapter.c plugin/bubbler.c plugin/bubbler_midi.c plugin/bubbler_art.c plugin/bubbler_dsp.c plugin/bubbler_params.c -o .test-build/test_adapter
.test-build/test_adapter
.test-build/test_adapter --snapshots > .test-build/view-states.jsonl

$CC -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I plugin tests/test_features.c plugin/bubbler_dsp.c plugin/bubbler_params.c -lm -o .test-build/test_features
.test-build/test_features

$CC -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I plugin tests/test_kernels.c plugin/bubbler_params.c -o .test-build/test_kernels
.test-build/test_kernels
