{ pkgs, ... }:
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
  system.build.installBootLoader = pkgs.writeScript "install-sbin-init.sh" ''
    #!${pkgs.runtimeShell}
    ${pkgs.coreutils}/bin/ln -fs "$1/init" /sbin/init
    ${pkgs.coreutils}/bin/cp "$1/nixctl" /nixctl
  '';
}
