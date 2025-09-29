{ pkgs, rm2-stuff, ... }:
let
  launcher = pkgs.callPackage ../pkgs/nixos-launcher.nix { inherit rm2-stuff; };
in

{

  system.systemBuilderCommands = ''
    cp ${launcher} $out/launch
  '';

  # Use /sbin/init as a bootloader.
  system.build.installBootLoader = pkgs.writeScript "install-sbin-init.sh" ''
    #!${pkgs.runtimeShell}
    ${pkgs.coreutils}/bin/ln -fs "$1/init" /sbin/init
    ${pkgs.coreutils}/bin/ln -fs "$1/launch" /launch
  '';
}
