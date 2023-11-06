#!/bin/sh

set -eux

DOCKER_IMAGE=$1
IPKS_PATH=$(readlink -f "$2")
HOST_BUILD_PATH=$(readlink -f "$3")


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

  "$HOST_BUILD_PATH"/tools/rm2fb-emu/rm2fb-test \
    127.0.0.1 8888 screenshot "${SH_PATH}"

  if ! diff "${SH_PATH}" "${ASSETS_DIR}/$NAME"
  then
    echo "Unmatching screenshot $NAME"
    exit 1
  fi
}

tap_at() {
  "$HOST_BUILD_PATH"/tools/rm2fb-emu/rm2fb-test \
    127.0.0.1 8888 touch "$1" "$2"
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
# do_ssh opkg install display || true # This fails as rm2fb fails to start.

do_ssh opkg install ./*.ipk

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
sleep 2
check_screenshot "yaft2.png"
tap_at 76 1832
tap_at 324, 1704
sleep 2
check_screenshot "startup.png"

# Xochitl
tap_at 628 1060
sleep 30
check_screenshot "xochitl.png"


echo "ALL OK"
