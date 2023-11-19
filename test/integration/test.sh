#!/bin/sh

set -eux

DOCKER_IMAGE=$1
IPKS_PATH=$(readlink -f "$2")
TEST_BINARY=$(readlink -f "$3")


TEST_DIR=$(dirname -- "$(readlink -f -- "$0")")
ASSETS_DIR="${TEST_DIR}/assets/"
TMP_DIR=$(mktemp -d)

if [ "$#" -ge 4 ]
then
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

  for matches in "$@"
  do
    if diff "${SH_PATH}" "${ASSETS_DIR}/$matches"
    then
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

image=$(docker run --name rm-docker --rm -d -p 2222:22 -p 8888:8888 "$DOCKER_IMAGE")
trap cleanup EXIT

while ! do_ssh true
do
  sleep 1
done

scp -P 2222 "$IPKS_PATH"/*.ipk root@localhost:

# Update & install display to make sure rm2fb.ipa replaces it.
do_ssh systemctl restart systemd-timesyncd
do_ssh opkg update

# Xochitl fails to install on 3.5 :(
do_ssh opkg install ./*.ipk || true
do_ssh systemctl daemon-reload

do_ssh systemctl start rm2fb
sleep 1
do_ssh systemctl start rocket

# rocket
sleep 2
check_screenshot "startup.png"

# tilem
tap_at 458 1062
sleep 2
check_screenshot "tilem.png"
tap_at 840 962
sleep 2
check_screenshot "startup.png"

# Yaft2
tap_at 930 1060
sleep 3
check_screenshot "yaft2.png"
tap_at 76 1832
tap_at 324, 1704
sleep 2
check_screenshot "startup.png"

# Xochitl
tap_at 628 1060
sleep 60

check_screenshot \
  "xochitl_2.15.png" \
  "xochitl_3.3.png" \
  "xochitl_3.5.png"

echo "ALL OK"
