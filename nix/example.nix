{
  lib,
  pkgs,
  rm2-stuff,
  ...
}:
{
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
  networking.wireless.enable = true;

  # Disable the flake registry of nixpkgs to save space
  nixpkgs.flake.setFlakeRegistry = false;
  nixpkgs.flake.setNixPath = false;

  system.stateVersion = "24.11";
}
