{
  writeShellApplication,
  util-linux,
  coreutils,
  rm2-stuff,
}:
writeShellApplication {
  name = "xochitl";

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
    mount /dev/mmcblk2p2 $root || mount -o ro /dev/mmcblk2p2 $root
    mount -o remount,ro $root $root
    mount -o ro /dev/mmcblk2p1 $root/var/lib/uboot

    # Mount /home rw, to allow document access.
    mount /dev/mmcblk2p4 $root/home

    # Mount special fs
    mount --rbind /dev $root/dev
    mount --rbind /sys $root/sys
    mount --rbind /proc $root/proc

    # /tmp needed for xochitl
    mount -t tmpfs none $root/tmp

    # Mount nix stuff, so the LD_PRELOAD library can be found.
    mount -o bind,ro /nix $root/nix

    # Mount /run for rm2fb.sock and dbus access.
    mount -o bind /run $root/run

    # Start xochitl
    exec chroot $root /usr/bin/env -i \
      PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin \
      LD_PRELOAD=${rm2-stuff.dev-rm2-toolchain.rm2display}/lib/librm2fb_client.so \
      /usr/bin/xochitl
  '';
}
