{
  pkgs,
  lib,
  config,
  rm2-stuff,
  ...
}:
let
  # TODO: use nixpkgs overlay
  rm2-pkgs = rm2-stuff.dev-cross;
  mkWrapper = pkgs.callPackage ../pkgs/wrapWithClient.nix { rm2-stuff = rm2-pkgs; };

  tilem = mkWrapper rm2-pkgs.tilem;
  yaft = mkWrapper rm2-pkgs.yaft;
  rocket = mkWrapper rm2-pkgs.rocket;
in
{
  options = {
    programs = {
      yaft = {
        enable = lib.mkEnableOption "Enable Yaft";
      };
      tilem = {
        enable = lib.mkEnableOption "Enable TilEm";
      };
      rocket = {
        enable = lib.mkEnableOption "Enable Rocket Launcher";
      };
    };
  };

  config = lib.mkMerge [
    (lib.mkIf config.programs.yaft.enable {
      environment.etc."draft/yaft.draft".text = ''
        name=yaft
        desc=Framebuffer terminal
        call=${lib.getExe' yaft "yaft"}
        term=:
        imgFile=yaft
      '';
      environment.etc."draft/icons/yaft.png".source = "${yaft}/etc/draft/icons/yaft.png";
      environment.systemPackages = [ yaft ];
    })

    (lib.mkIf config.programs.tilem.enable {
      environment.etc."draft/tilem.draft".text = ''
        name=TilEm
        desc=TI-84+ emulator
        call=${lib.getExe' tilem "tilem"}
        term=:
        imgFile=tilem
      '';
      environment.etc."draft/icons/tilem.png".source = "${tilem}/etc/draft/icons/tilem.png";
      environment.systemPackages = [ tilem ];
    })

    (lib.mkIf config.programs.rocket.enable {
      environment.systemPackages = [ rocket ];
      systemd.services."rocket" = {
        description = "Rocket launcher on boot";
        serviceConfig = {
          Type = "simple";
          # Kill yaft_reader that might still be running from the nixos launch script.
          ExecStartPre = "-${lib.getExe' pkgs.procps "pkill"} yaft_reader";
          ExecStart = lib.getExe' rocket "rocket";
        };

        environment = {
          HOME = "%h";
        };

        wantedBy = [ "multi-user.target" ];
      };
    })
  ];
}
