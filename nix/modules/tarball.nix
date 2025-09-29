{
  pkgs,
  config,
  modulesPath,
  ...
}:
{
  system.build.image = pkgs.callPackage "${modulesPath}/../lib/make-system-tarball.nix" {
    fileName = "rm-nixos";
    extraArgs = "--owner=0";

    storeContents = [
      {
        object = config.system.build.toplevel;
        symlink = "none";
      }
    ];

    contents = [
      {
        source = config.system.build.toplevel + "/launch";
        target = "/launch";
      }
      {
        source = config.system.build.toplevel + "/init";
        target = "/sbin/init";
      }
      {
        # This is required by systemd to recognize the /run/nextroot mount as
        # a linux distro.
        source = config.system.build.toplevel + "/etc/os-release";
        target = "/etc/os-release";
      }
    ];
  };

  boot.postBootCommands = ''
    # After booting, register the contents of the Nix store in the Nix
    # database.
    if [ -f /nix-path-registration ]; then
      ${config.nix.package.out}/bin/nix-store --load-db < /nix-path-registration &&

      # nixos-rebuild also requires a "system" profile
      ${config.nix.package.out}/bin/nix-env -p /nix/var/nix/profiles/system --set /run/current-system
      rm /nix-path-registration
    fi
  '';
}
