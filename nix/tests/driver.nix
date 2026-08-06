{
  lib,
  writeShellScript,
  coreutils,
  openimageio,
  openssh,

  vm,
  tools,

  testScript,
  golden ? false,
}:
let
  golden-files = ./assets;
in
writeShellScript "test-driver" ''
  export PATH="${
    lib.makeBinPath [
      vm
      tools
      coreutils
      openimageio
      openssh
    ]
  }"

  vmAddr="127.0.0.1 8888"
  mkdir -p $out/screensots $out/diffs

  in_nixos() {
    ssh -o StrictHostKeyChecking=no -i ${./id_ed25519} -p 2222 test@localhost "$@"
  }

  fail() {
    in_nixos journalctl --no-pager
    exit 1
  }

  check_screenshot() {
    NAME="$1"
    rm2fb-test $vmAddr screenshot $out/screensots/$NAME
    if ! idiff -o $out/diffs/$NAME $out/screensots/$NAME ${golden-files}/$NAME; then
      return 1
    fi
    return 0
  }

  wait_for() {
    NAME="$1"
    TIMEOUT=''${2:-5}
    sleep 1
    for _ in $(seq $TIMEOUT); do
      if check_screenshot "$NAME"; then
        rm -f $out/diffs/$NAME
        return 0
      fi
      sleep 1
    done
    ${if golden then "echo NOK" else "fail"}
  }

  # Like check_screenshot, but tolerant of extra lines having appeared above
  # the expected content (e.g. kernel noise such as "hrtimer: interrupt took
  # ..." warnings landing on /dev/kmsg under a loaded host, ahead of a
  # deliberate write to a kmsg-fed console) - the golden's expected content
  # is only required to appear starting at some whole-line offset, not
  # necessarily at row 0. Tries offsets 0, CELL_HEIGHT, 2*CELL_HEIGHT, ...
  # and accepts the first one that matches (within -allowfailures, since even
  # a correctly-aligned match isn't always byte-identical).
  CELL_HEIGHT=32

  check_screenshot_tolerant() {
    NAME="$1"
    MAX_LINES=''${2:-8}
    rm2fb-test $vmAddr screenshot $out/screensots/$NAME

    info=$(iinfo "${golden-files}/$NAME")
    info=''${info#*: }
    width=''${info%% x*}
    info=''${info#* x }
    height=''${info%%,*}

    for line in $(seq 0 $MAX_LINES); do
      lineShift=$((line * CELL_HEIGHT))
      h=$((height - lineShift))
      if [ "$h" -le 0 ]; then
        break
      fi

      oiiotool "$out/screensots/$NAME" --cut "''${width}x''${h}+0+''${lineShift}" \
        -o "$out/diffs/actual_$NAME" >/dev/null
      oiiotool "${golden-files}/$NAME" --cut "''${width}x''${h}+0+0" \
        -o "$out/diffs/golden_$NAME" >/dev/null

      if idiff -allowfailures 100 -o "$out/diffs/$NAME" \
          "$out/diffs/actual_$NAME" "$out/diffs/golden_$NAME"; then
        return 0
      fi
    done
    return 1
  }

  wait_for_tolerant() {
    NAME="$1"
    TIMEOUT=''${2:-5}
    sleep 1
    for _ in $(seq $TIMEOUT); do
      if check_screenshot_tolerant "$NAME"; then
        rm -f $out/diffs/$NAME
        return 0
      fi
      sleep 1
    done
    ${if golden then "echo NOK" else "fail"}
  }

  tap_at() {
    rm2fb-test $vmAddr pen "$1" "$2"
  }



  # Start the VM
  run_vm -daemonize

  (
    set -x
    # Run the test script
    ${testScript}
  )
''
