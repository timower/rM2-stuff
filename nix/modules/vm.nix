{
  lib,
  pkgs,
  options,
  config,
  ...
}:
let
  cfg = config.virtualisation;

  rm-emu =
    (import ../pkgs/rm-emu/default.nix {
      inherit lib;
      pkgs = cfg.host.pkgs;
      pkgsLinux = pkgs.pkgsBuildBuild;
    }).rm-emu;

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

  vm-fast = rm-emu.override (prev: {
    commandline = "console=ttymxc0 rootfstype=ext4 root=/dev/mmcblk2p4 rw rootwait init=/sbin/vm-init";
    rootfs = prev.rootfs.override {
      extraCommands = ''
        mkdir -p /mnt/home
        tar -C /mnt/home -xJf ${config.system.build.image}/tarball/*.tar.xz
        cp ${vm-init} /mnt/home/sbin/vm-init
      '';
    };
  });

  vm = rm-emu.override (prev: {
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

  options = {
    virtualisation.host.pkgs = lib.mkOption {
      type = options.nixpkgs.pkgs.type;
      default = pkgs.pkgsBuildBuild;
      defaultText = lib.literalExpression "pkgs.pkgsBuildBuild";
      example = lib.literalExpression ''
        import pkgs.path { system = "x86_64-darwin"; }
      '';
      description = ''
        Package set to use for the host-specific packages of the VM runner.
        Changing this to e.g. a Darwin package set allows running NixOS VMs on Darwin.
      '';
    };
  };

  config.system.build = {
    inherit vm vm-fast rm-emu;
  };
}
