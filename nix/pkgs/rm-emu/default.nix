{
  lib,
  pkgs,
  pkgsLinux,
}:
let
  kernel = pkgsLinux.callPackage ./kernel.nix { };

  versions = import ./versions.nix { inherit lib; };

  extractor = pkgsLinux.callPackage ./extractor.nix { };

  allRootFs = builtins.mapAttrs (
    fw_version: fw_info:
    pkgsLinux.callPackage ./rootfs.nix {
      inherit fw_version fw_info extractor;
    }
  ) versions;

  allRootFs' = lib.mapAttrs' (
    version: rootfs: lib.nameValuePair "rootfs-${version}" rootfs
  ) allRootFs;

  mkEmu = pkgs.callPackage ./rm-emu.nix;
  allEmus = lib.mapAttrs' (
    version: rootfs:
    lib.nameValuePair "rm-emu-${version}" (mkEmu {
      inherit kernel rootfs;
    })
  ) allRootFs;

  dockerImages = lib.mapAttrs' (
    _: pkg:
    lib.nameValuePair "docker-${pkg.version}" (
      pkgs.dockerTools.streamLayeredImage {
        name = "rm-emu";
        tag = "${pkg.version}";
        contents = [
          pkg
          pkgs.dockerTools.binSh
          pkgs.dockerTools.fakeNss
        ];

        config = {
          Cmd = [ "${pkg}/bin/run_vm" ];
        };
      }
    )
  ) allEmus;
in
{
  rm-emu-kernel = kernel;
  rm-emu-extractor = extractor;
  rm-emu = allEmus."rm-emu-3.20.0.92";
}
// allEmus
// allRootFs'
// dockerImages
