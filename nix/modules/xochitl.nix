{
  lib,
  pkgs,
  config,
  ...
}:
let
  xochitl-env = pkgs.callPackage ../pkgs/xochitlEnv.nix {
    preloadRm2fb = true;
  };
  xochitl = pkgs.writeShellApplication {
    name = "xochitl";
    runtimeInputs = [ pkgs.systemd ];
    text = ''
      systemctl start rm-sync
      ${lib.getExe xochitl-env} /usr/bin/xochitl
      systemctl stop rm-sync
    '';
  };

  xochitl-dbus = pkgs.writeTextFile {
    name = "xochitl-dbus-policy";
    text = ''
      <!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
        "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
      <busconfig>
        <policy user="root">
          <allow own="no.remarkable.sync" />
          <allow send_destination="no.remarkable.sync" />
        </policy>
        <policy context="default">
          <deny send_destination="no.remarkable.sync" />
        </policy>
      </busconfig>
    '';
    destination = "/share/dbus-1/system.d/no.remarkable.sync.conf";
  };
in
{
  options.programs.xochitl = {
    enable = lib.mkEnableOption "Enable Xochitl chroot wrapper";
  };

  config = lib.mkIf config.programs.xochitl.enable {

    environment.etc."draft/xochitl.draft".text = ''
      name=xochitl
      desc=Read documents and take notes
      call=${lib.getExe xochitl}
      term=:
      imgFile=xochitl
    '';
    environment.etc."draft/icons/xochitl.png".source = pkgs.fetchurl {
      url = "https://raw.githubusercontent.com/toltec-dev/toltec/9d15d2ddea4c58fc93e38f9ca0aed4d4afc5f9dc/package/xochitl/xochitl.png";
      hash = "sha256-ODuDGAe8VpZzyF9qDRbRC8tIYDQu4MjtTKb8dR9UZ8k=";
    };

    systemd.services.rm-sync = {
      description = "Helper for the rm-sync service";
      serviceConfig = {

        # Do NOT make this dbus, systemd will kill the service when it should be
        # running otherwise.
        Type = "simple";
        BusName = "no.remarkable.sync";
        ExecStart = "${lib.getExe xochitl-env} /usr/bin/rm-sync";
        # Restart = "on-failure";
        # RestartForceExitStatus=SIGHUP SIGINT SIGTERM SIGPIPE
      };
    };

    environment.systemPackages = [
      xochitl-env
      xochitl
    ];

    services.dbus.packages = [ xochitl-dbus ];
  };
}
