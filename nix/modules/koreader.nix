{
  lib,
  pkgs,
  config,
  ...
}:
let
  koreader = pkgs.callPackage ../pkgs/koreader.nix { };
in
{
  options.programs.koreader = {
    enable = lib.mkEnableOption "KOReader";
  };

  config = lib.mkIf config.programs.koreader.enable {
    environment.systemPackages = [ koreader ];
    environment.etc."draft/koreader.draft".text = ''
      name=KOReader
      desc=An ebook reader application supporting PDF, DjVu, EPUB, FB2 and many more formats
      call=${lib.getExe koreader}
      term=:
      imgFile=koreader
    '';
    environment.etc."draft/icons/koreader.png".source = "${koreader}/koreader/icon.png";
  };

}
