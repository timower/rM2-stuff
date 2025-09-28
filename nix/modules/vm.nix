{
  config,
  rm2-stuff,
  ...
}:
{
  imports = [ ];
  options = { };

  config = {
    system.build.vm = rm2-stuff.rm-emu.override (prev: {
      rootfs = prev.rootfs.override {
        extraCommands = ''
          mkdir -p /mnt/home/root/nixos
          tar -C /mnt/home/root/nixos -xJf ${config.system.build.image}/tarball/*.tar.xz

          # Mount home on startup, makes debugging faster.
          sed -i 's|/dev/unknown|/dev/mmcblk2p4|' /mnt/root/etc/fstab
        '';

      };
    });
  };
}
