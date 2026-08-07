# Upstream nixpkgs' own unit test for `system.build.etcActivationCommands`
# (nixos/modules/system/etc/test.nix) - same assertions (symlink target,
# file mode, content), but with our project's ../modules/etc.nix imported
# on top so its `mkForce` swaps in our Rust setup-etc instead of the
# default perl script. Verifies our implementation against upstream's own
# correctness test rather than just our own testScript-driven VM boot.
{
  pkgs,
  lib,
  evalMinimalConfig,
  pkgsModule,
  runCommand,
  vmTools,
  writeText,
}:
let
  # Stand in for nixpkgs' real services/system/userborn.nix module (which
  # etc.nix sets services.userborn.enable = mkDefault true against): the
  # real module's assertions reference config.systemd.*, which doesn't
  # exist in this minimal evalModules - a bare option declaration is
  # enough to let etc.nix's mkDefault merge cleanly.
  userbornStub =
    { lib, ... }:
    {
      options.services.userborn.enable = lib.mkOption {
        type = lib.types.bool;
        default = false;
      };
    };

  node = evalMinimalConfig (
    { config, ... }:
    {
      imports = [
        pkgsModule
        "${pkgs.path}/nixos/modules/system/etc/etc.nix"
        userbornStub
        ../modules/etc.nix
      ];
      environment.etc."passwd" = {
        text = passwdText;
      };
      environment.etc."hosts" = {
        text = hostsText;
        mode = "0751";
        # Named rather than the "+uid"/"+gid" default: exercises setup-etc's
        # getpwnam(3)/getgrnam(3) lookup path (uzers::get_user_by_name /
        # get_group_by_name), not just the numeric-literal fast path.
        user = "root";
        group = "root";
      };
    }
  );
  passwdText = ''
    root:x:0:0:System administrator:/root:/run/current-system/sw/bin/bash
  '';
  hostsText = ''
    127.0.0.1 localhost
    ::1 localhost
    # testing...
  '';
in
vmTools.runInLinuxVM (
  runCommand "test-etc-rust-vm" { } ''
    mkdir -p /etc
    # Seed a real root entry ahead of running the activation script itself:
    # the "hosts" entry's user/group = "root" above needs getpwnam(3)/
    # getgrnam(3) to resolve "root" *while* setup-etc runs, and the /etc/etc
    # tree's own "passwd"/"group" entries (which would otherwise be the only
    # source for that) aren't guaranteed to be linked into place before
    # "hosts" is processed - directory walk order isn't sorted.
    echo "root:x:0:0:root:/root:/bin/sh" > /etc/passwd
    echo "root:x:0:" > /etc/group
    ${node.config.system.build.etcActivationCommands}
    set -x
    [[ -L /etc/passwd ]]
    diff /etc/passwd ${writeText "expected-passwd" passwdText}
    [[ 751 = $(stat --format %a /etc/hosts) ]]
    [[ 0 = $(stat --format %u /etc/hosts) ]]
    diff /etc/hosts ${writeText "expected-hosts" hostsText}
    set +x
    touch $out
  ''
)
