# Fork of the upstream getty module, to disable vt getty.
{
  lib,
  pkgs,
  config,
  ...
}:
with lib;
let
  cfg = config.services.getty;

  baseArgs = [
    "--login-program"
    "${cfg.loginProgram}"
  ]
  ++ optionals (cfg.autologinUser != null && !cfg.autologinOnce) [
    "--autologin"
    cfg.autologinUser
  ]
  ++ optionals (cfg.loginOptions != null) [
    "--login-options"
    cfg.loginOptions
  ]
  ++ cfg.extraArgs;

  gettyCmd = args: "${lib.getExe' pkgs.util-linux "agetty"} ${escapeShellArgs baseArgs} ${args}";
in
{
  console.enable = false;

  # Note: this is set here rather than up there so that changing
  # nixos.label would not rebuild manual pages
  services.getty.greetingLine = mkDefault ''<<< Welcome to ${config.system.nixos.distroName} ${config.system.nixos.label} (\m) - \l >>>'';
  services.getty.helpLine = mkIf (
    config.documentation.nixos.enable && config.documentation.doc.enable
  ) "\nRun 'nixos-help' for the NixOS manual.";

  systemd.additionalUpstreamSystemUnits = [
    "getty.target"
    "getty-pre.target"
    "serial-getty@.service"
  ];

  systemd.services."serial-getty@" = {
    serviceConfig.ExecStart = [
      "" # override upstream default with an empty ExecStart
      (gettyCmd "%I --keep-baud $TERM")
    ];
    restartIfChanged = false;
  };
}
