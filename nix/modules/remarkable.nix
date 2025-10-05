{ modulesPath, ... }:
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

  # Auto restart wpa_supplicant as it can fail after suspend.
  systemd.services.wpa_supplicant.serviceConfig.Restart = "on-failure";
  systemd.services.wpa_supplicant.serviceConfig.RestartSec = "10";
  # systemd.services.wpa_supplicant.startLimitBurst = 5;
  # systemd.services.wpa_supplicant.startLimitIntervalSec = 600;

  # Use networkd.
  networking.useNetworkd = true;

  # So we can copy xochitl config to /etc/wpa_supplicant.conf
  networking.wireless.allowAuxiliaryImperativeNetworks = true;

  # Firwall doesn't work anyway as the kernel doesn't support nft.
  networking.firewall.enable = false;

  # We soft-reboot into nixos, no bootloader needed.
  boot.loader.grub.enable = false;
  boot.bootspec.enable = false;

  # Kernel and init are managed by xochitl 'host'.
  boot.kernel.enable = false;
  boot.initrd.enable = false;
  systemd.shutdownRamfs.enable = false;

  # Cross compile for armv7l on x86_64.
  nixpkgs.hostPlatform.system = "armv7l-linux";
  nixpkgs.buildPlatform.system = "x86_64-linux";
}
