{
  pkgs,
  lib,
  config,
  ...
}:
let
  rm2-pkgs = pkgs.rm2-stuff;
  mkWrapper = pkgs.callPackage ../pkgs/wrapWithClient.nix { };

  tilem = mkWrapper rm2-pkgs.tilem;
  yaft = mkWrapper rm2-pkgs.yaft;
  rocket = mkWrapper rm2-pkgs.rocket;

  rocketUser = config.programs.rocket.loginUser;
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
        loginUser = lib.mkOption {
          type = lib.types.nullOr lib.types.str;
          default = "root";
          description = ''
            Username of the account that will be automatically logged in.
          '';
        };
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
      systemd.services."rocket" = lib.mkMerge [
        {
          description = "Rocket launcher";
          serviceConfig = {
            Type = "notify";
            # Kill yaft_reader that might still be running from the nixos launch script.
            ExecStartPre = "-${lib.getExe' pkgs.procps "pkill"} yaft_reader";
            ExecStart = lib.getExe' rocket "rocket";

            Restart = "on-failure";
            RestartSec = "5";
          };

          wantedBy = [ "multi-user.target" ];
          aliases = [ "launcher.service" ];

          # Don't kill the launcher on config changes.
          restartIfChanged = false;
        }
        (lib.mkIf (rocketUser == null) {
          environment = {
            HOME = "%h";
          };
        })
        (lib.mkIf (rocketUser != null) {
          serviceConfig = {
            User = rocketUser;
            WorkingDirectory = "~";
            PAMName = "login";
            UnsetEnvironment = "TERM";

            # Grab tty1, this will allow us to suspend via polkit.
            TTYPath = "/dev/tty1";
            StandardOutput = "journal";
          };

          # conflicts = [ "getty@tty1.service" ];
          after = [
            # "getty@tty1.service"
            "systemd-user-sessions.service"
          ];
        })
      ];
    })

    (lib.mkIf (rocketUser != null) {
      # Enable polkit, so users can suspend.
      security.polkit.enable = true;
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
