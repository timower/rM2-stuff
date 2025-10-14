{
  lib,
  pkgs,
  rm2-stuff,
}:
let
  system = lib.nixosSystem {
    modules = [
      ../modules/remarkable.nix
      ../example.nix
    ];
  };
  vm = system.config.system.build.vm-fast;
in
{
  basic = pkgs.runCommand "basic-test" { } ''
    set -eux

    export PATH="${
      lib.makeBinPath [
        vm
        rm2-stuff.tools
        pkgs.coreutils
      ]
    }"

    run_vm -daemonize
    vmAddr="127.0.0.1 8888"

    while ! rm2fb-test $vmAddr screenshot /dev/null; do
      sleep 5
    done

    touch $out
  '';

}
