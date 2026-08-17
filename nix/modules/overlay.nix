{ lib, ... }:
{
  nixpkgs.overlays = [
    (
      final: prev:
      let
        # rm2-stuff's apps/yaft needs ghostty-vt (see nix/pkgs/ghostty-vt.nix
        # and flake.nix's `default`/`dev-cross`, which wire it the same way
        # for the plain package builds) -- cross-target it to match whatever
        # this system itself is being built for. `final` here already *is*
        # the hostPlatform=armv7 package set, so zig -- which must run on
        # the build machine, not the target, to cross-emit armv7 object code
        # -- has to come from final.buildPackages, not final directly (which
        # would build an armv7-native zig binary, pointless and, as found by
        # actually building this, much more expensive).
        ghostty-vt = final.callPackage ../pkgs/ghostty-vt.nix {
          zig = final.buildPackages.callPackage ../pkgs/zig_0_16.nix { };
          zigTargetFlags = lib.optionals prev.stdenv.hostPlatform.isArmv7 [
            "-Dtarget=arm-linux-gnueabihf"
            "-Dcpu=cortex_a7"
          ];
        };
      in
      {
        rm2-stuff = final.callPackage ../pkgs/rm2-stuff.nix { inherit ghostty-vt; };
      }
      // lib.optionalAttrs prev.stdenv.hostPlatform.isArmv7 {

        # curl's `wcurl` wrapper script keeps the build machine's shebang
        # (`#!/nix/store/...-bash-.../bin/sh`) in cross builds, since
        # fixupPhase's patchShebangs runs before the cross output exists to
        # repoint it at - unusable on the target and drags a whole extra
        # x86_64 bash + glibc closure into the system for nothing. Fixed
        # upstream by https://github.com/NixOS/nixpkgs/commit/341ec4198d06f7312d63b970d92292be38ca2b7a,
        # not yet in nixos-25.11.
        curl = prev.curl.overrideAttrs (old: {
          postFixup = (old.postFixup or "") + ''
            rm -f "$bin/bin/wcurl"
          '';
        });

        systemd = prev.systemd.override {
          pname = "systemd-minimized";

          # Usesful stuff
          withAnalyze = true;
          withHwdb = true;
          withResolved = true;
          withShellCompletions = true;
          withPam = true;
          withPolkit = true;

          withLogind = true;
          withNetworkd = true;
          withNss = true;

          withTimedated = true;
          withTimesyncd = true;

          withCoredump = true;

          # Maybe useful / needed?
          withHostnamed = true;
          withLocaled = true;

          # Libraries / features, might be useful?
          withOpenSSL = true;
          withPCRE2 = true;
          withCompression = true;
          withLibarchive = true;

          withAcl = false;
          withApparmor = false;
          withAudit = false;
          withCryptsetup = false;
          withRepart = false;
          withDocumentation = false;
          withEfi = false;
          withFido2 = false;
          withGcrypt = false;
          withHomed = false;
          withImportd = false;
          # withIptables = false;
          withKmod = false;
          withLibBPF = false;
          withLibidn2 = false;
          withMachined = false;
          withOomd = false;
          withPortabled = false;
          withRemote = false;
          withSysupdate = false;
          withSysusers = false;
          withTpm2Tss = false;
          withUserDb = false;
          withUkify = false;
          withBootloader = false;
          withPasswordQuality = false;
          withVmspawn = false;
          withQrencode = false;
        };
      }
    )
  ];
}
