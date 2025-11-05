{
  pkgs,
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
  services = {
    rm2fb.enable = true;
    openssh.enable = true;

    getty.autologinUser = "rM";
  };

  programs = {
    yaft.enable = true;
    tilem.enable = true;
    rocket.enable = true;
    xochitl.enable = true;
    koreader.enable = true;
  };

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
  networking.wireless.enable = true;

  # Disable the flake registry of nixpkgs to save space
  nixpkgs.flake.setFlakeRegistry = false;
  nixpkgs.flake.setNixPath = false;

  system.stateVersion = "24.11";
}
