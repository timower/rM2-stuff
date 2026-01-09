{ lib, pkgs, ... }:
let
  launcher = pkgs.callPackage ../pkgs/nixos-launcher.nix { };
in

{
  # Make sure /nixctl is created in the toplevel derivation.
  system.systemBuilderCommands = ''
    cp ${launcher} $out/nixctl
  '';

  # Use /sbin/init as a bootloader.
  # Copy nixctl, as /nix won't be bind mounted yet.
  system.build.installBootLoader = pkgs.writeShellScript "install-sbin-init.sh" ''
    export PATH="${
      lib.makeBinPath [
        pkgs.coreutils
        pkgs.findutils
      ]
    }"

    ln -fs "$1/init" /sbin/init
    cp "$1/nixctl" /nixctl

    for profile in /nix/var/nix/profiles/system-*-link; do
      systemPath=$(readlink "$profile")
      systemName=$(basename "$profile")
      ln -fs "$systemPath/init" "/sbin/init-$systemName"
    done

    find /sbin/ -xtype l -delete
  '';
}
