{
  lib,
  pkgs,
  config,
  ...
}:
let
  pkg = pkgs.callPackage ../pkgs/xochitlEnv.nix { };
in
{
  options.programs.xochitl = {
    enable = lib.mkEnableOption "Enable Xochitl chroot wrapper";
  };

  config = lib.mkIf config.programs.xochitl.enable {
    environment.etc."draft/xochitl.draft".text = ''
      name=xochitl
      desc=Read documents and take notes
      call=${lib.getExe pkg}
      term=:
      imgFile=xochitl
    '';
    environment.etc."draft/icons/xochitl.png".source = pkgs.fetchurl {
      url = "https://raw.githubusercontent.com/toltec-dev/toltec/9d15d2ddea4c58fc93e38f9ca0aed4d4afc5f9dc/package/xochitl/xochitl.png";
      hash = "sha256-ODuDGAe8VpZzyF9qDRbRC8tIYDQu4MjtTKb8dR9UZ8k=";
    };
    environment.systemPackages = [ pkg ];
  };
}
