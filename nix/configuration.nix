{
  config,
  lib,
  pkgs,
  modulesPath,
  rm2-stuff,
  ...
}:
let
  launcher = pkgs.callPackage ./pkgs/nixos-launcher.nix { inherit rm2-stuff; };
in
{
  imports = [
    "${modulesPath}/profiles/minimal.nix"
    ./modules/vm.nix
    ./modules/security-wrappers
  ];

  # Disable the flake registry of nixpkgs to save space
  nixpkgs.flake.setFlakeRegistry = false;
  nixpkgs.flake.setNixPath = false;

  fileSystems."/" = {
    # Root is data partition.
    device = "/dev/mmcblk2p4";
    fsType = "ext4";
  };

  environment.systemPackages = with pkgs; [
    htop
    gdb
    strace
    rm2-stuff.dev-cross
  ];

  users.mutableUsers = false;
  users.users."rM" = {
    isNormalUser = true;
    extraGroups = [
      "wheel"

      # Needed to access /dev/input/event devices
      "input"
    ];

    password = "rM";
  };
  security.sudo.wheelNeedsPassword = false;

  # Add sudo users as trusted, so nixos-rebuild works.
  nix.settings.trusted-users = [ "@wheel" ];

  systemd.services."yaft-boot" = {
    description = "Yaft terminal on boot";
    serviceConfig = {
      Type = "simple";
      ExecStartPre = "-${lib.getExe' pkgs.procps "pkill"} yaft_reader";
      ExecStart = lib.getExe' rm2-stuff.dev-cross "yaft";
    };

    environment = {
      LD_PRELOAD = "${rm2-stuff.dev-cross}/lib/librm2fb_client.so";
    };

    wantedBy = [ "multi-user.target" ];
  };

  services.openssh.enable = true;

  # Firwall doesn't work anyway as the kernel doesn't support nft.
  networking.firewall.enable = false;
  networking.useNetworkd = true;
  networking.wireless = {
    enable = true;
    # So we can copy xochitl config to /etc/wpa_supplicant.conf
    allowAuxiliaryImperativeNetworks = true;
  };

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
        source = config.system.build.toplevel + "/launcher";
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

  system.systemBuilderCommands = ''
    cp ${launcher} $out/launch
  '';

  # Use /sbin/init as a bootloader.
  system.build.installBootLoader = pkgs.writeScript "install-sbin-init.sh" ''
    #!${pkgs.runtimeShell}
    ${pkgs.coreutils}/bin/ln -fs "$1/init" /sbin/init
    ${pkgs.coreutils}/bin/ln -fs "$1/launch" /launch
  '';

  # We soft-reboot into nixos, no bootloader needed.
  boot.loader.grub.enable = false;
  boot.bootspec.enable = false;

  # Kernel and init are managed by xochitl 'host'.
  boot.kernel.enable = false;
  boot.initrd.enable = false;

  # Cross compile for armv7l on x86_64.
  nixpkgs.hostPlatform.system = "armv7l-linux";
  nixpkgs.buildPlatform.system = "x86_64-linux";

  system.stateVersion = "24.11";
}
