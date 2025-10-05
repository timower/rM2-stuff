{
  lib,
  pkgs,
  config,
  rm2-stuff,
  ...
}:
let
  vm-init = pkgs.writeScript "vm-stage-1" ''
    #!${pkgs.runtimeShell}
    export PATH=${
      lib.makeBinPath [
        pkgs.util-linux
        pkgs.coreutils
      ]
    }

    mkdir -p /proc /sys /dev
    mount -t proc none /proc
    mount -t sysfs none /sys

    mount -t devtmpfs devtmpfs /dev
    ln -s /proc/self/fd /dev/fd
    ln -s /proc/self/fd/0 /dev/stdin
    ln -s /proc/self/fd/1 /dev/stdout
    ln -s /proc/self/fd/2 /dev/stderr

    exec /sbin/init
  '';

  vm-fast = rm2-stuff.rm-emu.override (prev: {
    commandline = "console=ttymxc0 rootfstype=ext4 root=/dev/mmcblk2p4 rw rootwait init=/sbin/vm-init";
    rootfs = prev.rootfs.override {
      extraCommands = ''
        mkdir -p /mnt/home
        tar -C /mnt/home -xJf ${config.system.build.image}/tarball/*.tar.xz
        cp ${vm-init} /mnt/home/sbin/vm-init
      '';
    };
  });

  vm = rm2-stuff.rm-emu.override (prev: {
    rootfs = prev.rootfs.override {
      extraCommands = ''
        mkdir -p /mnt/home/root/nixos
        tar -C /mnt/home/root/nixos -xJf ${config.system.build.image}/tarball/*.tar.xz

        # Mount home on startup, makes debugging faster.
        sed -i 's|/dev/unknown|/dev/mmcblk2p4|' /mnt/root/etc/fstab
      '';

    };
  });
in
{
  imports = [ ];
  options = { };

  config.system.build = {
    inherit vm vm-fast;
  };
}
