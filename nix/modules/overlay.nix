{ lib, ... }:
{
  nixpkgs.overlays = [
    (import ../overlays/ghostty-vt.nix)
    (
      final: prev:
      {
        # apps/yaft needs ghostty-vt, see nix/overlays/ghostty-vt.nix.
        rm2-stuff = final.callPackage ../pkgs/rm2-stuff.nix { };
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
