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
