{
  lib,
  fetchurl,
  runCommand,

  # libguestfs-with-appliance,
  vmTools,
  qemu,
  cpio,
  util-linux,
  dosfstools,
  e2fsprogs,

  versions,
  extractor,
}:
{
  rootfs = builtins.mapAttrs (
    fw_version: info:
    let
      inherit (info) fileName fileHash;

      base_url = "https://updates-download.cloud.remarkable.engineering/build/reMarkable%20Device%20Beta/RM110"; # Default URL for v2 versions
      base_url_v3 = "https://updates-download.cloud.remarkable.engineering/build/reMarkable%20Device/reMarkable2";

      isNewFormat = (builtins.compareVersions fw_version "3.11.2.5") == 1;

      url =
        if isNewFormat then
          "https://storage.googleapis.com/remarkable-versions/${fileName}"
        else if (builtins.compareVersions fw_version "3.0.0.0") == 1 then
          "${base_url_v3}/${fw_version}/${fw_version}_reMarkable2-${fileName}.signed"
        else
          "${base_url}/${fw_version}/${fw_version}_reMarkable2-${fileName}.signed";

      updateArchive = fetchurl {
        inherit url;
        sha256 = fileHash;
      };

      rootfsImageOld = runCommand "rm-rootfs.ext4" { nativeBuildInputs = [ extractor ]; } ''
        extractor ${updateArchive} $out
      '';

      rootfsImageNew = runCommand "rm-rootfs.ext4" { nativeBuildInputs = [ cpio ]; } ''
        cpio -i --file ${updateArchive}
        gzip -dc ${lib.strings.removeSuffix ".swu" fileName}.ext4.gz > $out
      '';

      rootfsImage = if isNewFormat then rootfsImageNew else rootfsImageOld;

      needsPatchDhcpcd = (builtins.compareVersions fw_version "3.12.0.0") != 1;

    in
    vmTools.runInLinuxVM (
      runCommand "rm-${fw_version}.qcow2"
        {
          passthru.fw_version = fw_version;

          buildInputs = [
            util-linux
            dosfstools
            e2fsprogs
          ];
          preVM = ''
            ${qemu}/bin/qemu-img create -f qcow2 $out 8G
          '';
          QEMU_OPTS = "-drive file=\"$out\",if=virtio,cache=unsafe,werror=report,format=qcow2";
        }
        ''
          set -x

          sfdisk /dev/vda <<SFD
          label: dos
          label-id: 0xc410b303
          device: /dev/vda
          unit: sectors
          sector-size: 512

          /dev/vda1 : start=        2048, size=       40960, type=83
          /dev/vda2 : start=       43008, size=      552960, type=83
          /dev/vda3 : start=      595968, size=      552960, type=83
          /dev/vda4 : start=     1148928, size=    13793280, type=83
          SFD

          mkfs.vfat /dev/vda1

          cp ${rootfsImage} /dev/vda2
          e2fsck -f -y /dev/vda2
          resize2fs /dev/vda2

          mkfs.ext4 /dev/vda3
          mkfs.ext4 /dev/vda4

          mkdir -p /mnt/home /mnt/root
          mount /dev/vda2 /mnt/root
          mount /dev/vda4 /mnt/home

          cp -a /mnt/root/etc/skel /mnt/home/root
          umount /mnt/home

          ln -s /dev/null /mnt/root/etc/systemd/system/remarkable-fail.service

          ${lib.optionalString needsPatchDhcpcd "sed -i 's/wlan/eth/' /mnt/root/lib/systemd/system/dhcpcd.service"}

          # 3.22 stopped shipping dropbear.socket on all interfaces.
          if ! [ -e /mnt/root/lib/systemd/system/dropbear.socket ]; then
            cp /mnt/root/lib/systemd/system/dropbear-usb0.socket /mnt/root/lib/systemd/system/dropbear.socket
            sed -i 's/.*usb0.*//' /mnt/root/lib/systemd/system/dropbear.socket
            ln -s /lib/systemd/system/dropbear.socket /mnt/root/etc/systemd/system/sockets.target.wants/
          fi

          umount /mnt/root

          set +x
        ''
    )
  ) versions;
}
