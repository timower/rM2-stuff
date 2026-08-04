{
  lib,
  pkgs,
  config,
  ...
}:
let
  # Matches services.rm2fb.variant (nix/modules/rm2-display.nix) - the
  # "swtcon" variant needs xochitl to preload the coexistence client
  # (ClientSwtcon.cpp) instead of the by-address hooking one, so its own
  # internal swtcon runs untouched rather than being hooked at all.
  xochitl-env = pkgs.callPackage ../pkgs/xochitlEnv.nix {
    preloadRm2fb = true;
    preloadLibs = [
      "/run/current-system/sw/lib/librm2fb_client.so"
    ]
    ++ config.programs.xochitl.extraPreloadLibraries;
  };
  xochitl = pkgs.writeShellApplication {
    name = "xochitl";
    runtimeInputs = with pkgs; [
      systemd
      util-linux
    ];
    text = ''
      exec {fd}>"''${XDG_RUNTIME_DIR:-/run}/xochitl.lock"
      if ! flock -n -x $fd; then
        echo "Only one xochitl supported!"
        exit 1
      fi

      ctlCmd="systemctl"
      if [ "$EUID" -ne "0" ]; then
        ctlCmd="systemctl --user"
      fi
      $ctlCmd start rm-sync
      ${lib.getExe xochitl-env} /usr/bin/xochitl
      $ctlCmd stop rm-sync
    '';
  };

  xochitl-dbus = pkgs.writeTextFile {
    name = "xochitl-dbus-policy";
    text = ''
      <!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
        "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
      <busconfig>
        <policy context="default">
          <allow own="no.remarkable.sync" />
          <allow send_destination="no.remarkable.sync" />
        </policy>
      </busconfig>
    '';
    destination = "/share/dbus-1/system.d/no.remarkable.sync.conf";
  };

  wpa-dbus = pkgs.writeTextFile {
    name = "wpa-dbus-policy";
    text = ''
      <!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
       "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
      <busconfig>
        <policy context="mandatory">
          <allow send_destination="fi.w1.wpa_supplicant1"/>
          <allow send_interface="fi.w1.wpa_supplicant1"/>
          <allow receive_sender="fi.w1.wpa_supplicant1" receive_type="signal"/>
        </policy>
      </busconfig>
    '';
    destination = "/share/dbus-1/system.d/wpa-dbus.conf";
  };

  rm-sync-service = {
    description = "Helper for the rm-sync service";
    serviceConfig = {

      # Do NOT make this dbus, systemd will kill the service when it should be
      # running otherwise.
      Type = "simple";
      BusName = "no.remarkable.sync";
      ExecStart = "${lib.getExe xochitl-env} /usr/bin/rm-sync";
    };
  };
in
{
  options.programs.xochitl = {
    enable = lib.mkEnableOption "Enable Xochitl chroot wrapper";

    extraPreloadLibraries = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ ];
      description = ''
        Extra libraries (full store paths) to LD_PRELOAD into xochitl,
        after the rm2fb client library. Test/debugging use only - e.g.
        libioctl-dump.so to mock /dev/fb0 for the services.rm2fb.variant =
        "swtcon" coexistence client in an environment without a real
        framebuffer device (see nix/tests/default.nix's xochitl-swtcon) -
        not meant for a real on-device configuration.
      '';
    };
  };

  config = lib.mkIf config.programs.xochitl.enable {

    # We need sudo to start the xochitl-env script as root.
    # Security wrappers doesn't work with shell scripts.
    security.sudo = {
      enable = true;
      extraRules = [
        {
          users = [ "ALL" ];
          commands = [
            {
              command = lib.getExe xochitl-env;
              options = [ "NOPASSWD" ];
            }
          ];
        }
      ];
    };

    # xochitl ultimately runs as an unprivileged user here (rocket.service's
    # User=, still in effect after xochitl-env's sudo+chroot --userspec
    # dance drops back to it) - fine for the "hook" variant, since xochitl's
    # own display code never actually runs there (fully intercepted by
    # address before it would reach a scheduling call). The "swtcon"
    # variant's whole point is letting xochitl's real, unmodified code run
    # instead, including its own pthread_setschedparam(SCHED_FIFO) call for
    # its display threads - which fails ("Unable to set thread priority:
    # Operation not permitted") and crashes without this. RLIMIT_RTPRIO
    # lets an unprivileged process use real-time scheduling up to this
    # value without needing CAP_SYS_NICE - this is real hardware behavior,
    # not just a VM/test artifact, since the same de-privileged launch path
    # is used either way.
    security.pam.loginLimits = lib.mkIf (config.services.rm2fb.variant == "swtcon") [
      {
        domain = "*";
        type = "-";
        item = "rtprio";
        value = "99";
      }
    ];

    environment = {
      etc."draft/xochitl.draft".text = ''
        name=xochitl
        desc=Read documents and take notes
        call=${lib.getExe xochitl}
        term=:
        imgFile=xochitl
      '';
      etc."draft/icons/xochitl.png".source = pkgs.fetchurl {
        url = "https://raw.githubusercontent.com/toltec-dev/toltec/9d15d2ddea4c58fc93e38f9ca0aed4d4afc5f9dc/package/xochitl/xochitl.png";
        hash = "sha256-ODuDGAe8VpZzyF9qDRbRC8tIYDQu4MjtTKb8dR9UZ8k=";
      };

      systemPackages = [
        xochitl-env
        xochitl
      ];
    };

    # Add rm-sync as both a user and system service
    systemd.user.services.rm-sync = rm-sync-service;
    systemd.services.rm-sync = rm-sync-service;

    services.dbus.packages = [
      xochitl-dbus
      wpa-dbus
    ];
  };
}
