#!/bin/sh

set -eux

DOCKER_IMAGE=$1
IPKS_PATH=$2
HOST_BUILD_PATH=$3

image=$(docker run --rm -d -p 2222:22 -p 8888:8888 "$DOCKER_IMAGE")

cleanup() {
  docker kill "$image"
}
# trap cleanup EXIT

do_ssh() {
  ssh -o StrictHostKeyChecking=no -p 2222 root@localhost /bin/sh -l -c "'$*'"
}

while ! do_ssh true
do
  sleep 1
done

scp -P 2222 "$IPKS_PATH"/*.ipk root@localhost:

# Update & install display to make sure rm2fb.ipa replaces it.
do_ssh opkg update
do_ssh opkg install display || true # This fails as rm2fb fails to start.

do_ssh opkg install *.ipk

do_ssh rm2fb-forward &

sleep 1

"$HOST_BUILD_PATH"/tools/rm2fb-emu/rm2fb-emu 127.0.0.1 8888 &

do_ssh env LD_PRELOAD=/opt/lib/librm2fb_client.so /usr/bin/xochitl


echo "ALL OK"
