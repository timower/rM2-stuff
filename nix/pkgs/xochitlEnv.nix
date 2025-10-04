{
  lib,

  writeShellApplication,
  util-linux,
  coreutils,

  preloadRm2fb ? false,
  extraEnv ? { },
}:
let
  envAttr = {
    PATH = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin";
    HOME = "/home/root";
  }
  // extraEnv
  // lib.optionalAttrs preloadRm2fb { LD_PRELOAD = "/run/current-system/sw/lib/librm2fb_client.so"; };

  envList = lib.attrsets.mapAttrsToList (name: value: "${name}=${value}") envAttr;
in
writeShellApplication {
  name = "xochitl-env";

  runtimeInputs = [
    util-linux
    coreutils
  ];

  text = ''
    if [ "$EUID" -ne 0 ]; then
      echo "Not running as root. Re-executing with sudo..."
      exec sudo "$0" "$@"
    fi

    # Unshare the mount namespace, so mounts don't leak and are cleaned up.
    # Adapted from nixos-enter.
    if [ -z "''${UNSHARED:-}" ]; then
      export UNSHARED=1
      exec unshare --fork --mount -- "$0" "$@"
    else
      mount --make-rprivate /
    fi

    root=/run/xochitlEnv

    mkdir -p $root

    # Mount partitions, read only to prevent changes.
    activePartition=$(cat /run/active-partition)
    mount "$activePartition" $root
    mount -o remount,ro,bind $root
    mount -o ro /dev/mmcblk2p1 $root/var/lib/uboot

    # Mount /home rw, to allow document access.
    mount /dev/mmcblk2p4 $root/home

    # Mount special fs
    mount --rbind /dev $root/dev
    mount --rbind /sys $root/sys
    mount --rbind /proc $root/proc

    # Mount nix stuff, so the LD_PRELOAD library can be found.
    mount -o bind,ro /nix $root/nix

    # /tmp needed for xochitl, and the rm-sync service
    # rm.synchronizer: Synchronizer's libraryLock path="/tmp/library-enumeration.lock"
    mount -o bind /tmp $root/tmp
    # Mount /run for rm2fb.sock and dbus access.
    mount -o bind /run $root/run

    # Start xochitl
    exec chroot $root /usr/bin/env -i ${lib.escapeShellArgs envList} "$@"
  '';
}
