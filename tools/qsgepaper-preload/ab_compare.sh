#!/bin/sh
# Black-box A/B comparison between the native swtcon implementation and the
# real libqsgepaper.so (via SWTCON_LIBIMPL=1), run against a real device or
# the emulator over ssh. Uploads qsgepaper-test/libioctl-dump.so/
# pan-capture-compare, runs qsgepaper-test once per implementation with
# SWTCON_PAN_CAPTURE set, then diffs the two captured display sequences. See
# AGENTS.md's "A/B comparison (SWTCON_LIBIMPL + pan-capture)" section for the
# technique and its caveats - this is a coarse smoke test on the *set* of
# distinct content ever displayed, not a byte-exact correctness oracle.
#
# Usage: tools/qsgepaper-preload/ab_compare.sh <ssh-target> [test-case]
#
#   <ssh-target>  ssh destination for the device/emulator (e.g. "RemEmu"),
#                 passed straight through to ssh/scp.
#   [test-case]   optional, forwarded to qsgepaper-test as its test-case
#                 argument (see main.cpp): omit to run every test, "0" to
#                 run init only, "N" (>=1) to run just test case N - handy
#                 for isolating one test instead of comparing the whole
#                 suite at once (e.g. to rule out known-racy tests like
#                 case 8, "burst of un-synced overlapping updates").
#
# Requires build/dev to already contain qsgepaper-test, libioctl-dump.so and
# pan-capture-compare - run `ninja -C build/dev` first.

set -eu

if [ "$#" -lt 1 ]; then
  echo "usage: $0 <ssh-target> [test-case]" >&2
  exit 2
fi

TARGET="$1"
TEST_CASE="${2:-}"

REPO_ROOT=$(cd "$(dirname -- "$0")/../.." && pwd)
BUILD_DIR="${REPO_ROOT}/build/dev"

QSGEPAPER_TEST="${BUILD_DIR}/tools/qsgepaper-preload/qsgepaper-test"
LIBIOCTL_DUMP="${BUILD_DIR}/tools/ioctl-dump/libioctl-dump.so"
PAN_CAPTURE_COMPARE="${BUILD_DIR}/tools/ioctl-dump/pan-capture-compare"

for f in "$QSGEPAPER_TEST" "$LIBIOCTL_DUMP" "$PAN_CAPTURE_COMPARE"; do
  if [ ! -e "$f" ]; then
    echo "missing $f - run 'ninja -C build/dev' first" >&2
    exit 1
  fi
done

REMOTE_DIR="/home/root"
NATIVE_CAPTURE="/tmp/ab_compare_native.txt"
LIB_CAPTURE="/tmp/ab_compare_lib.txt"

echo "Uploading test binaries to ${TARGET}:${REMOTE_DIR} ..." >&2
scp "$QSGEPAPER_TEST" "$LIBIOCTL_DUMP" "$PAN_CAPTURE_COMPARE" "${TARGET}:${REMOTE_DIR}/"

echo "Running native implementation ..." >&2
ssh "$TARGET" "cd ${REMOTE_DIR} && yes '' | SWTCON_PAN_CAPTURE=${NATIVE_CAPTURE} LD_PRELOAD=./libioctl-dump.so ./qsgepaper-test ${TEST_CASE} >/dev/null"

echo "Running library implementation (SWTCON_LIBIMPL=1) ..." >&2
ssh "$TARGET" "cd ${REMOTE_DIR} && yes '' | SWTCON_LIBIMPL=1 SWTCON_PAN_CAPTURE=${LIB_CAPTURE} LD_PRELOAD=./libioctl-dump.so ./qsgepaper-test ${TEST_CASE} >/dev/null"

echo "Comparing captured display sequences ..." >&2
ssh "$TARGET" "cd ${REMOTE_DIR} && ./pan-capture-compare ${NATIVE_CAPTURE} ${LIB_CAPTURE}"
