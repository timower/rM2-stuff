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
      NOTIFY_SOCKET = null;
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

  clientLibrary =
    if config.services.rm2fb.variant == "swtcon" then
      "librm2fb_client_swtcon.so"
    else
      "librm2fb_client.so";
  rm2fb-client = pkgs.runCommand "rm2fb-client" { } ''
    mkdir -p $out/lib
    ln -s ${pkgs.rm2-stuff.rm2display}/lib/${clientLibrary} $out/lib/librm2fb_client.so
    ln -s ${pkgs.rm2-stuff.rm2display}/bin $out/bin
  '';

  serverExec =
    if config.services.rm2fb.variant != "hook" then
      lib.getExe' rm2-server-pkg "rm2fb_server_swtcon"
    else
      "${lib.getExe xochitl-env} ${lib.getExe' rm2-server-pkg "rm2fb_server"}";
in
{
  options = {
    services.rm2fb = {
      # TODO: enable by default?
      enable = lib.mkEnableOption "Enable own rm2fb server";

      swtcon = lib.mkEnableOption "Enable own swtcon server";

      variant = lib.mkOption {
        type = lib.types.enum [
          "hook"
          "swtcon-server"
          "swtcon"
        ];
        default = "hook";
        description = ''
          Which rm2fb server/client implementation to run:

          - "hook": Use hooks for xochitl to drive the display and intercept updates.
          - "swtcon-server": Use native swtcon implementation on the server.
          - "swtcon": Swtcon on the server, and hookless (own) swtcon based in xochitl.
        '';
      };
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
          ListenDatagram = [ "/run/rm2fb.control.sock" ];
          ListenStream = [
            "/run/rm2fb.sock"
            "8888"
          ];
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

          ExecStart = serverExec;

          Restart = "on-failure";
          RestartSec = "5";
        };
      };

    })

    (lib.mkIf config.hardware.rm2display.enable {
      # Add the client to the systemPacakges, this ensure it's linked into
      # /run/current-system/sw/lib.
      environment.systemPackages = [ rm2fb-client ];
    })

  ];
}
