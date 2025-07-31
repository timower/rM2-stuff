#!/bin/sh

set -eux

DOCKER_IMAGE=$1
IPKS_PATH=$(readlink -f "$2")
TEST_BINARY=$(readlink -f "$3")

TEST_DIR=$(dirname -- "$(readlink -f -- "$0")")
ASSETS_DIR="${TEST_DIR}/assets/"
TMP_DIR=$(mktemp -d)

if [ "$#" -ge 4 ]; then
  TMP_DIR=$(readlink -f "$4")
fi

cleanup() {
  docker kill "$image"
}

do_ssh() {
  ssh -o StrictHostKeyChecking=no -p 2222 root@localhost /bin/sh -l -c "'$*'"
}

check_screenshot() {
  NAME="$1"
  SH_PATH="${TMP_DIR}/${NAME}"

  "$TEST_BINARY" 127.0.0.1 8888 screenshot "${SH_PATH}"

  for matches in "$@"; do
    if diff "${SH_PATH}" "${ASSETS_DIR}/$matches"; then
      return
    else
      echo "Unmatching screenshot $matches"
    fi
  done

  exit 1
}

tap_at() {
  "$TEST_BINARY" 127.0.0.1 8888 touch "$1" "$2"
}

press_power() {
  "$TEST_BINARY" 127.0.0.1 8888 power
}

image=$(docker run --name rm-docker --rm -d -p 2222:22 -p 8888:8888 "$DOCKER_IMAGE")
# trap cleanup EXIT

while ! do_ssh true; do
  sleep 1
done

scp -P 2222 "$IPKS_PATH"/*.ipk root@localhost:

# do_ssh timedatectl status
do_ssh systemctl restart systemd-timesyncd || do_ssh systemctl restart systemd-timedated
do_ssh opkg update

# TODO: xochitl doesn't configure for 3.5+
do_ssh opkg install ./*.ipk calculator mines

# Start rocket, which should trigger the rm2fb socket and start the service.
do_ssh systemctl start rocket

# rocket
sleep 2
check_screenshot "startup.png"

# tilem
tap_at 730 1050
sleep 2
check_screenshot "tilem.png"
tap_at 840 962
sleep 2
check_screenshot "startup.png"

# Yaft
tap_at 1058 1042
sleep 4
check_screenshot "yaft.png"
tap_at 76 1832
sleep 1
tap_at 324 1704
sleep 2
check_screenshot "startup.png"

# Calculator
# Does not work on 3.20 as Qt5 isn't used anymore.
if do_ssh test -e /usr/lib/libQt5Quick.so.5; then
  tap_at 400 1054
  sleep 3
  check_screenshot "calculator.png"
  tap_at 826 1440
  sleep 1
  check_screenshot "calculator_3.png"

  press_power
  sleep 1
  tap_at 702 718 # Stop sleeping
  sleep 1
  tap_at 824 1124 # Kill calculator
  sleep 1
  check_screenshot "startup.png"
fi

# mines
tap_at 600 1054
sleep 2
check_screenshot "mines.png"

press_power
sleep 1
tap_at 702 718 # Stop sleeping
sleep 2
tap_at 764 1124 # Kill mines
sleep 1
check_screenshot "startup.png"

# Xochitl
tap_at 832 1086
sleep 120

check_screenshot \
  "xochitl_2.15.png" \
  "xochitl_3.3.png" \
  "xochitl_3.5.png" \
  "xochitl_3.8.png" \
  "xochitl_3.20.png"

echo "ALL OK"
