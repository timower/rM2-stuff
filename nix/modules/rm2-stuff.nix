{
  pkgs,
  lib,
  config,
  ...
}:
let
  # TODO: use nixpkgs overlay
  rm2-pkgs = pkgs.rm2-stuff;
  mkWrapper = pkgs.callPackage ../pkgs/wrapWithClient.nix { };

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
      environment = {
        etc."draft/yaft.draft".text = ''
          name=yaft
          desc=Framebuffer terminal
          call=${lib.getExe' yaft "yaft"}
          term=:
          imgFile=yaft
        '';
        etc."draft/icons/yaft.png".source = "${yaft}/etc/draft/icons/yaft.png";
        systemPackages = [ yaft ];
      };
    })

    (lib.mkIf config.programs.tilem.enable {
      environment = {
        etc."draft/tilem.draft".text = ''
          name=TilEm
          desc=TI-84+ emulator
          call=${lib.getExe' tilem "tilem"}
          term=:
          imgFile=tilem
        '';
        etc."draft/icons/tilem.png".source = "${tilem}/etc/draft/icons/tilem.png";
        systemPackages = [ tilem ];
      };
    })

    (lib.mkIf config.programs.rocket.enable {
      environment.systemPackages = [ rocket ];
      systemd.services."rocket" = {
        description = "Rocket launcher";
        serviceConfig = {
          Type = "simple";
          # Kill yaft_reader that might still be running from the nixos launch script.
          ExecStartPre = "-${lib.getExe' pkgs.procps "pkill"} yaft_reader";
          ExecStart = lib.getExe' rocket "rocket";

          Restart = "on-failure";
          RestartSec = "5";
        };

        environment = {
          HOME = "%h";
        };

        wantedBy = [ "multi-user.target" ];
        aliases = [ "launcher.service" ];
      };
    })
  ];
}

# systemd.services."yaft-boot" = {
#   description = "Yaft terminal on boot";
#   serviceConfig = {
#     Type = "simple";
#     ExecStartPre = "-${lib.getExe' pkgs.procps "pkill"} yaft_reader";
#     ExecStart = lib.getExe' rm2-stuff.dev-cross.yaft "yaft";
#   };

#   environment = {
#     LD_PRELOAD = "/run/current-system/sw/lib/librm2fb_client.so";
#   };

#   wantedBy = [ "multi-user.target" ];
# };
