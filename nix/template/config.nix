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
    openssh.enable = true;
  };

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

  # Wifi: nixctl copies your saved networks from xochitl's config on
  # every boot, so pick the option matching the xochitl version you're on.

  # xochitl < 3.28, reads /home/root/.config/remarkable/wifi_networks.conf.
  networking.wireless.enable = true;

  # xochitl >= 3.28 (beta), reads
  # /home/root/.config/NetworkManager/system-profiles/*.nmconnection.
  # NetworkManager only lets members of the "networkmanager" group talk to
  # it over D-Bus, so add "rM" (or whichever user runs xochitl) to it.
  # networking.networkmanager.enable = true;
  # users.users."rM".extraGroups = [ "networkmanager" ];

  # Disable the flake registry of nixpkgs to save space
  nixpkgs.flake.setFlakeRegistry = false;
  nixpkgs.flake.setNixPath = false;

  system.stateVersion = "25.11";
}
