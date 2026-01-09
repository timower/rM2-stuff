{ lib, ... }:
{
  nixpkgs.overlays = [
    (
      final: prev:
      {
        rm2-stuff = final.callPackage ../pkgs/rm2-stuff.nix { };
      }
      // lib.optionalAttrs prev.stdenv.hostPlatform.isArmv7 {

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
