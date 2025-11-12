{
  lib,
  pkgs,
  config,
  ...
}:
let
  rm2-pkgs = pkgs.rm2-stuff;
  rm2-server-pkg = rm2-pkgs.rm2display;

  xochitl-env = pkgs.callPackage ../pkgs/xochitlEnv.nix {
    # No need to add the client for the rm2fb server.
    preloadRm2fb = false;

    # Pass through the socket activation env vars.
    extraEnv = {
      LISTEN_PID = null;
      LISTEN_FDS = null;
      LISTEN_FDNAMES = null;

      LD_LIBRARY_PATH = "/usr/lib";
    };
  };

  kill-rm2fb = pkgs.writeShellScript "kill-rm2fb" ''
    export PATH="${
      lib.makeBinPath [
        pkgs.procps
        pkgs.coreutils
      ]
    }"
    pkill rm2fb_server || exit 0

    while pgrep rm2fb_server; do
      sleep 1
    done
  '';

in
{
  options = {
    services.rm2fb = {
      # TODO: enable by default?
      enable = lib.mkEnableOption "Enable own rm2fb server";
    };

    hardware.rm2display = {
      enable = lib.mkEnableOption "Enable rm2fb client wrapper" // {
        default = true;
      };
    };

  };

  config = lib.mkMerge [

    (lib.mkIf config.services.rm2fb.enable {
      systemd.sockets."rm2fb" = {
        description = "rM2 framebuffer server socket";
        before = [ "launcher.service" ];

        socketConfig = {
          ExecStartPre = [
            # Kill yaft_reader started before the soft-reboot.
            "-${lib.getExe' pkgs.procps "pkill"} yaft_reader"
            # Kill rm2fb_server started before the soft-reboot.
            "-${kill-rm2fb}"
          ];

          # No access control on the socket, so users can access it.
          SocketMode = "0777";
          ListenDatagram = "/run/rm2fb.sock";

          ListenStream = "8888";
          ReusePort = "true";
        };

        wantedBy = [ "sockets.target" ];
      };

      systemd.services."rm2fb" = {
        description = "rM2 Framebuffer server";
        before = [ "launcher.service" ];

        startLimitIntervalSec = 600;
        startLimitBurst = 4;

        # Don't take down the rm2fb server on config changes, as it'd break
        # any running clients.
        restartIfChanged = false;

        serviceConfig = {
          Type = "simple";

          ExecStart = "${lib.getExe xochitl-env} ${lib.getExe' rm2-server-pkg "rm2fb_server"}";

          Restart = "on-failure";
          RestartSec = "5";
        };
      };

    })

    (lib.mkIf config.hardware.rm2display.enable {
      # Add the client to the systemPacakges, this ensure it's linked into
      # /run/current-system/sw/lib.
      environment.systemPackages = [ rm2-pkgs.rm2display ];
    })

  ];
}
