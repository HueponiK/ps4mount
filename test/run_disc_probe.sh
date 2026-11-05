#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/test
${CC:-cc} -D_POSIX_C_SOURCE=200809L -O1 -g -Wall -Wextra -Werror -Isrc \
  test/disc_probe_test.c src/disc_probe.c -o build/test/disc_probe_test
build/test/disc_probe_test
${CC:-cc} -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -Werror -Isrc \
