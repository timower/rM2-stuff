#!/bin/sh
# Black-box A/B comparison between the native swtcon implementation and the
# real libqsgepaper.so (via SWTCON_LIBIMPL=1), run against a real device or
# the emulator over ssh. Uploads qsgepaper-test/libioctl-dump.so/
# pan-capture-compare, runs qsgepaper-test once per implementation with
# SWTCON_PAN_CAPTURE set, then diffs the two captured display sequences. See
# AGENTS.md's "A/B comparison (SWTCON_LIBIMPL + pan-capture)" section for the
# technique and its caveats.
#
# pan-capture-compare does an ORDERED (positional) hash compare, which only
# holds up when comparing a single, isolated test case - a whole-suite run's
# frame count varies with real-time pacing (see main.cpp/pan_capture_compare
# comments). So by default (no test-case argument), this script runs every
# test case *individually* (its own pair of captures each), skipping test
# case 8 ("burst of un-synced overlapping updates") - it deliberately races
# the worker thread with no swtcon_wait() between submissions, so it is
# known-flaky under any A/B comparison and not a real regression signal (see
# CLAUDE.md's Phase 9 notes). Pass "8" explicitly to test it anyway.
#
# Usage: tools/qsgepaper-preload/ab_compare.sh <ssh-target> [test-case]
#
#   <ssh-target>  ssh destination for the device/emulator (e.g. "RemEmu"),
#                 passed straight through to ssh/scp.
#   [test-case]   optional. Omit to run every test case except the known-
#                 flaky case 8, comparing each in isolation and reporting a
#                 pass/fail summary. "0" runs init only. "N" (1-10) runs and
#                 compares just that one test case (including "8", if you
#                 want to see its known flakiness).
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

echo "Uploading test binaries to ${TARGET}:${REMOTE_DIR} ..." >&2
scp "$QSGEPAPER_TEST" "$LIBIOCTL_DUMP" "$PAN_CAPTURE_COMPARE" "${TARGET}:${REMOTE_DIR}/"

# Runs one test case's native-vs-library capture and compare. Sets $RUN_ONE_RC
# to pan-capture-compare's exit status explicitly (rather than relying on
# the calling context's set -e/if-test interaction, which differs across
# /bin/sh implementations) - upload and both capture runs are still expected
# to hard-fail the whole script via set -e if they error out for real.
run_one() {
  case_arg="$1"
  native_capture="/tmp/ab_compare_native_${case_arg:-all}.txt"
  lib_capture="/tmp/ab_compare_lib_${case_arg:-all}.txt"

  echo "-- test case ${case_arg:-all (whole suite)} --" >&2
  echo "Running native implementation ..." >&2
  ssh "$TARGET" "cd ${REMOTE_DIR} && yes '' | SWTCON_PAN_CAPTURE=${native_capture} LD_PRELOAD=./libioctl-dump.so ./qsgepaper-test ${case_arg} >/dev/null"

  echo "Running library implementation (SWTCON_LIBIMPL=1) ..." >&2
  ssh "$TARGET" "cd ${REMOTE_DIR} && yes '' | SWTCON_LIBIMPL=1 SWTCON_PAN_CAPTURE=${lib_capture} LD_PRELOAD=./libioctl-dump.so ./qsgepaper-test ${case_arg} >/dev/null"

  echo "Comparing captured display sequences ..." >&2
  RUN_ONE_RC=0
  ssh "$TARGET" "cd ${REMOTE_DIR} && ./pan-capture-compare ${native_capture} ${lib_capture}" || RUN_ONE_RC=$?
}

if [ -n "$TEST_CASE" ]; then
  run_one "$TEST_CASE"
  exit "$RUN_ONE_RC"
fi

# No test case given: run every case except the known-flaky case 8, and
# report a pass/fail summary across all of them.
FAILED=""
for n in 1 2 3 4 5 6 7 9 10; do
  run_one "$n"
  if [ "$RUN_ONE_RC" -eq 0 ]; then
    echo "case $n: MATCH" >&2
  else
    echo "case $n: MISMATCH" >&2
    FAILED="$FAILED $n"
  fi
done

echo >&2
if [ -z "$FAILED" ]; then
  echo "All test cases matched (case 8 skipped - known flaky, pass it explicitly to check anyway)." >&2
  exit 0
else
  echo "Mismatched test case(s):$FAILED" >&2
  exit 1
fi
