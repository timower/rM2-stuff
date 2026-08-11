{
  lib,
  pkgs,
  ...
}:
let
  xochitl-env = pkgs.callPackage ../pkgs/xochitlEnv.nix { };
in
{
  # Always registered (not gated behind programs.xochitl.enable), since
  # services.rm2fb.variant = "hook" (nix/modules/rm2-display.nix) needs it
  # independently of whether xochitl itself is enabled.
  security.wrappers.xochitl-env = {
    setuid = true;
    owner = "root";
    group = "root";
    source = lib.getExe xochitl-env;
  };
}
