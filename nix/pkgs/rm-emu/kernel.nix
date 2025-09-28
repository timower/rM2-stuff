{
  lib,
  pkgsCross,
}:
let
  kernelNoMods = pkgsCross.armv7l-hf-multiplatform.linux_5_4.overrideAttrs (
    final: prev: {
      # Add a device tree with machine name set to 'reMarkable 2.0'
      preConfigure = ''
        cp ${./imx7d-rm.dts} arch/arm/boot/dts/imx7d-rm.dts
        sed -i 's/imx7d-sbc-imx7.dtb/imx7d-sbc-imx7.dtb imx7d-rm.dtb/' arch/arm/boot/dts/Makefile
      '';

      # Disable all modules
      postConfigure = ''
        sed -i 's/=m/=n/' $buildRoot/.config
      '';
    }
  );
in
kernelNoMods.override {
  defconfig = "imx_v6_v7_defconfig";

  autoModules = false;

  # TODO: figure out which setting is needed for docker builds to work.
  # enableCommonConfig = false;

  structuredExtraConfig = with lib.kernel; {
    INPUT_UINPUT = yes;
    KERNEL_LZO = no;
    KERNEL_GZIP = yes;

    NLS_CODEPAGE_437 = lib.mkForce yes;
    NLS_ISO8859_1 = lib.mkForce yes;

    # rM kernels don't have this :(
    TMPFS_XATTR = lib.mkForce no;
    TMPFS_POSIX_ACL = lib.mkForce no;
  };
}
