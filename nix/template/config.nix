{
  pkgs,
  ...
}:
{
  environment.systemPackages = with pkgs; [
    htop
  ];

  hardware.rm2display.enable = true;
  services = {
    rm2fb.enable = true;
    rm2fb.variant = "swtcon";
    openssh.enable = true;
  };

  programs.xochitl.extraPreloadLibraries = [
    "${pkgs.rm2-stuff.ioctl_dump}/lib/libioctl-dump.so"
  ];

  programs = {
    yaft.enable = true;
    tilem.enable = true;
    xochitl.enable = true;
    koreader.enable = true;

    rocket = {
      enable = true;
      loginUser = "rM";
    };
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

  # Add sudo users as trusted, so nixos-rebuild works.
  nix.settings.trusted-users = [ "@wheel" ];
  networking.wireless.enable = true;

  # Disable the flake registry of nixpkgs to save space
  nixpkgs.flake.setFlakeRegistry = false;
  nixpkgs.flake.setNixPath = false;

  system.stateVersion = "25.11";
}
