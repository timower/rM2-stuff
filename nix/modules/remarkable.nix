{ modulesPath, lib, ... }:
{
  imports = [
    "${modulesPath}/profiles/minimal.nix"
    ./vm.nix
    ./security-wrappers
    ./tarball.nix
    ./launch.nix
    ./rm2-display.nix
    ./rm2-stuff.nix
    ./xochitl.nix
    ./koreader.nix
    ./overlay.nix
    ./console.nix
  ];

  fileSystems."/" = {
    # Root is data partition.
    device = "/dev/mmcblk2p4";
    fsType = "ext4";
  };

  # make logind ignore the power key, the launcher handles this.
  services.logind.powerKey = "ignore";
  environment.etc."systemd/system-sleep/sleep-wifi.sh" = {
    mode = "555";
    text = ''
      #!/bin/sh

      if [ "$1" == "pre" ]; then
      	echo "$(basename $0): Shutting down Wifi" > /dev/kmsg
        ip link set wlan0 down
        echo mmc1:0001:2 > /sys/bus/sdio/drivers/brcmfmac/unbind
        echo mmc1:0001:1 > /sys/bus/sdio/drivers/brcmfmac/unbind
      elif [ "$1" == "post" ]; then
      	echo "$(basename $0): Restoring Wifi" > /dev/kmsg
      	echo mmc1:0001:1 > /sys/bus/sdio/drivers/brcmfmac/bind
      	echo mmc1:0001:2 > /sys/bus/sdio/drivers/brcmfmac/bind
      fi
    '';
  };

  systemd = {
    # Auto restart wpa_supplicant as it can fail after suspend.
    services.wpa_supplicant.serviceConfig.Restart = "on-failure";
    services.wpa_supplicant.serviceConfig.RestartSec = "10";
    # services.wpa_supplicant.startLimitBurst = 5;
    # services.wpa_supplicant.startLimitIntervalSec = 600;
    shutdownRamfs.enable = false;

    oomd.enable = false;
    suppressedSystemUnits = [
      # Kernel module loading.
      "systemd-modules-load.service"
      "kmod-static-nodes.service"
      "modprobe@.service"
    ];
  };

  networking = {

    # Use networkd.
    useNetworkd = true;

    # So we can copy xochitl config to /etc/wpa_supplicant.conf
    wireless.allowAuxiliaryImperativeNetworks = true;

    # Firwall doesn't work anyway as the kernel doesn't support nft.
    firewall.enable = false;
  };

  boot = {
    bcache.enable = false;

    # We soft-reboot into nixos, no bootloader needed.
    loader.grub.enable = false;
    bootspec.enable = false;

    # Kernel and init are managed by xochitl 'host'.
    kernel.enable = false;
    initrd.enable = false;
  };

  system = {
    rebuild.enableNg = false;
    tools = {
      nixos-generate-config.enable = false;
      nixos-option.enable = false;
    };
  };

  # Cross compile for armv7l on x86_64 by default.
  nixpkgs.hostPlatform.system = "armv7l-linux";
  nixpkgs.buildPlatform = lib.mkDefault "x86_64-linux";
}
