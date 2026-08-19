{
  lib,
  pkgs,
  options,
  config,
  extendModules,
  ...
}:
let
  cfg = config.virtualisation;
  emu-pkg = import ../pkgs/rm-emu {
    inherit lib;
    inherit (cfg.host) pkgs;
    pkgsLinux = pkgs.pkgsBuildBuild;
  };

  rm-emu = emu-pkg."rm-emu-${cfg.rmEmuVersion}";

  # apps/yaft needs ghostty-vt to build at all (see nix/pkgs/ghostty-vt.nix);
  # cfg.host.pkgs.callPackage builds rm2-stuff natively for the host below,
  # so this is the native (non-cross) variant, same as flake.nix's `default`.
  host-ghostty-vt = cfg.host.pkgs.callPackage ../pkgs/ghostty-vt.nix {
    zig = cfg.host.pkgs.callPackage ../pkgs/zig_0_16.nix { };
  };

  vm-config = extendModules {
    modules = [
      {
        systemd.targets.sleep.enable = false;

        # rm-emu has no wifi iface, so wpa_supplicant's interface
        # auto-detect (nixpkgs' wpa_supplicant.nix) hangs on udevadm
        # forever, and xochitl's dbus Get eats dbus-daemon's fixed 25s
        # activation timeout waiting for it to claim the bus name.
        # Skip the wait: -u alone starts it with zero interfaces and
        # claims the bus name immediately.
        systemd.services.wpa_supplicant.script = lib.mkForce ''
          touch /etc/wpa_supplicant.conf
          exec wpa_supplicant -u -s -c /etc/wpa_supplicant.conf
        '';
      }
    ];
  };

  # Statically linked: vm-init runs before /nix/store is mounted (that's its
  # job), so it - and everything it calls - can't depend on the store.
  busybox-static = pkgs.busybox.override { enableStatic = true; };

  vm-init = pkgs.writeScript "vm-stage-1" ''
    #!/sbin/busybox sh
    b=/sbin/busybox

    $b mkdir -p /proc /sys /dev
    $b mount -t proc none /proc
    $b mount -t sysfs none /sys

    $b mount -t devtmpfs devtmpfs /dev
    $b ln -s /proc/self/fd /dev/fd
    $b ln -s /proc/self/fd/0 /dev/stdin
    $b ln -s /proc/self/fd/1 /dev/stdout
    $b ln -s /proc/self/fd/2 /dev/stderr

    $b mkdir -p /nix/store
    $b mount -t 9p -o trans=virtio,version=9p2000.L,msize=262144,cache=loose,ro nix-store /nix/store

    # The toplevel to boot comes from the kernel command line, not baked in
    # here - that keeps this script (and so the disk image it's shipped on)
    # unchanged across nixos config edits, so only the cheap kernel-cmdline
    # substitution in rm-emu.nix's wrapper needs to rebuild, not the image
    # itself (which is a whole builder-VM boot via vmTools.runInLinuxVM).
    for arg in $($b cat /proc/cmdline); do
      case "$arg" in
        toplevel=*) toplevel=''${arg#toplevel=} ;;
      esac
    done

    exec "$toplevel/init"
  '';

  vm-nixos = rm-emu.override (prev: {
    commandline = "console=ttymxc0 rootfstype=ext4 root=/dev/mmcblk2p4 rw rootwait init=/sbin/vm-init toplevel=${vm-config.config.system.build.toplevel}";
    virtfsStore = builtins.storeDir;
    rootfs = prev.rootfs.override {
      extraCommands = ''
        mkdir -p /mnt/home/nix/store /mnt/home/sbin
        cp ${busybox-static}/bin/busybox /mnt/home/sbin/busybox
        cp ${vm-init} /mnt/home/sbin/vm-init
      '';
    };
  });

  vm-xochitl = rm-emu.override (prev: {
    rootfs = prev.rootfs.override {
      extraCommands = ''
        mkdir -p /mnt/home/root/nixos
        tar -C /mnt/home/root/nixos -xJf ${vm-config.config.system.build.image}/tarball/*.tar.xz

        # Mount home on startup, makes debugging faster.
        sed -i 's|/dev/unknown|/dev/mmcblk2p4|' /mnt/root/etc/fstab
      '';

    };
  });

  vm = cfg.host.pkgs.writeScriptBin "run-vm" ''
    #!/bin/sh

    export PATH="${
      lib.makeBinPath [
        (cfg.host.pkgs.callPackage ../pkgs/rm2-stuff.nix { ghostty-vt = host-ghostty-vt; }).tools
        vm-nixos
      ]
    }:$PATH"

    vmAddr="127.0.0.1 8888"

    waitForEmu() {
      while ! rm2fb-test $vmAddr screenshot /dev/null; do
        sleep 5
      done
      rm2fb-emu $vmAddr
    }

    waitForEmu &
    run_vm -serial mon:stdio
    wait
  '';

in
{
  imports = [ ];

  options = {
    virtualisation.host.pkgs = lib.mkOption {
      inherit (options.nixpkgs.pkgs) type;
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

    virtualisation.rmEmuVersion = lib.mkOption {
      type = lib.types.str;
      default = "3.23.0.64";
      example = "3.27.1.0";
      description = ''
        reMarkable firmware version (a key of nix/pkgs/rm-emu/versions.nix)
        whose extracted rootfs/kernel the emulator VM boots - this is what
        determines which real (closed-source) /usr/bin/xochitl binary
        programs.xochitl actually runs.
      '';
    };
  };

  config.system.build = {
    inherit
      vm-xochitl
      vm-nixos
      rm-emu
      vm
      vm-config
      ;
  };
}
