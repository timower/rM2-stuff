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
    ${if golden then "echo NOK" else "exit 1"}

  }

  tap_at() {
    rm2fb-test $vmAddr pen "$1" "$2"
  }

  # Start the VM
  run_vm -daemonize

  while ! rm2fb-test $vmAddr screenshot /dev/null; do
    sleep 5
  done

  (
    set -x
    # Run the test script
    ${testScript}
  )
''
