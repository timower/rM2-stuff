#!/usr/bin/bash

set -eux

FB_IN=$1
SCREEN_OUT=$2

RGB_OUT=$(mktemp)

scp "$FB_IN" "$RGB_OUT"
ffmpeg -vcodec rawvideo -f rawvideo -pix_fmt rgb565 -s 1404x1872 -i "$RGB_OUT" -f image2 -vcodec png "$SCREEN_OUT"
rm "$RGB_OUT"
