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
    evtest
  ];

  hardware.rm2display.enable = true;

  programs.yaft.enable = true;
  programs.tilem.enable = true;
  programs.rocket.enable = true;
  programs.xochitl.enable = true;
  programs.koreader.enable = true;

  users.mutableUsers = false;
  users.users."rM" = {
    isNormalUser = true;
    extraGroups = [
      "wheel"

      # Needed to access /dev/input/event devices
      "input"
    ];

    # password = "rM";
    hashedPassword = "$2b$05$nBj.3OinubU4iNP8aZ6tOOln8gJ68fjWPmCR1jNGzylVY5gHYOkUO";
  };
  security.sudo.wheelNeedsPassword = false;

  # Add sudo users as trusted, so nixos-rebuild works.
  nix.settings.trusted-users = [ "@wheel" ];

  # systemd.services."yaft-boot" = {
  #   description = "Yaft terminal on boot";
  #   serviceConfig = {
  #     Type = "simple";
  #     ExecStartPre = "-${lib.getExe' pkgs.procps "pkill"} yaft_reader";
  #     ExecStart = lib.getExe' rm2-stuff.dev-cross.yaft "yaft";
  #   };

  #   environment = {
  #     LD_PRELOAD = "${rm2-stuff.dev-cross.rm2display}/lib/librm2fb_client.so";
  #   };

  #   wantedBy = [ "multi-user.target" ];
  # };

  services.openssh.enable = true;
  networking.wireless.enable = true;

  # Disable the flake registry of nixpkgs to save space
  nixpkgs.flake.setFlakeRegistry = false;
  nixpkgs.flake.setNixPath = false;

  system.stateVersion = "24.11";
}
