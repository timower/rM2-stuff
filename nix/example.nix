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

  services.rm2fb.enable = true;
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

    password = "rM";
  };
  security.sudo.wheelNeedsPassword = false;
  services.getty.autologinUser = "rM";

  # Add sudo users as trusted, so nixos-rebuild works.
  nix.settings.trusted-users = [ "@wheel" ];

  services.openssh.enable = true;
  networking.wireless.enable = true;

  # Disable the flake registry of nixpkgs to save space
  nixpkgs.flake.setFlakeRegistry = false;
  nixpkgs.flake.setNixPath = false;

  system.stateVersion = "24.11";
}
