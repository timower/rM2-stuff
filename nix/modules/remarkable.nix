{ modulesPath, ... }:
{
  imports = [
    "${modulesPath}/profiles/minimal.nix"
    ./vm.nix
    ./security-wrappers
    ./tarball.nix
    ./launch.nix
  ];

  fileSystems."/" = {
    # Root is data partition.
    device = "/dev/mmcblk2p4";
    fsType = "ext4";
  };

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

  # Cross compile for armv7l on x86_64.
  nixpkgs.hostPlatform.system = "armv7l-linux";
  nixpkgs.buildPlatform.system = "x86_64-linux";
}
