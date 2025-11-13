{
  lib,
  pkgs,
  pkgsCross,
}:
let
  config = pkgs.stdenvNoCC.mkDerivation {
    name = "rm-defconfig";
    version = "5.4.70_v1.6";
    src = pkgs.fetchurl {
      url = "https://raw.githubusercontent.com/reMarkable/linux/d54fe67bf86e918468b936f97a2ec39f4f87a3d9/arch/arm/configs/zero-sugar_defconfig";
      hash = "sha256-t1L4QscCMO8Zo62fKfaWq95O4rF0we3GRq/EvpxazXw=";
    };

    buildCommand = ''
      cat $src > $out

      # Add E1000E so qemu ethernet works.
      echo 'CONFIG_E1000E=y' >> $out
      sed -i 's/# CONFIG_ETHERNET/CONFIG_ETHERNET=y/' $out

      # rM rootfs modules won't match anyway, so disable.
      sed -i 's/=m/=n/' $out

      # Disable some extra firmware set in the defconfig.
      sed -i 's/CONFIG_EXTRA_FIRMWARE=/#CONFIG_EXTRA_FIRMWARE=/' $out

      # Add uinput, rM has it as a module.
      sed -i 's/CONFIG_INPUT_UINPUT=n/CONFIG_INPUT_UINPUT=y/' $out
    '';
  };

  kernelNoMods = pkgsCross.armv7l-hf-multiplatform.linuxManualConfig rec {
    version = "5.4.70";
    src = pkgs.fetchurl {
      url = "https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-${version}.tar.xz";
      hash = "sha256-wLPYCFxbojXfOLALdA4FNllwnopcohlXojn2vCLEUAc=";
    };
    configfile = config;

    allowImportFromDerivation = true;
    kernelPatches = [
      {
        name = "compile-fix";
        patch = ./kernel-compile-fix.patch;
      }
    ];
  };
in
kernelNoMods.overrideAttrs (
  final: prev: {
    # Add a device tree with machine name set to 'reMarkable 2.0'
    preConfigure = ''
      cp ${./imx7d-rm.dts} arch/arm/boot/dts/imx7d-rm.dts
      sed -i 's/imx7d-sbc-imx7.dtb/imx7d-sbc-imx7.dtb imx7d-rm.dtb/' arch/arm/boot/dts/Makefile
    '';

    nativeBuildInputs = prev.nativeBuildInputs ++ [ pkgs.lzop ];
  }
)
