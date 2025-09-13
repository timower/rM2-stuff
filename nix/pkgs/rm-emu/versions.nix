{
  lib,
}:
let
  codexUpdateData = lib.importJSON (
    builtins.fetchurl {
      url = "https://raw.githubusercontent.com/Jayy001/codexctl/refs/tags/1752948641/data/version-ids.json";
      sha256 = "sha256-zN4Ztx7RYNhv4gDeHzk5UXLYflznuu51Q3h3UQUnw0Y=";
    }
  );
in
builtins.mapAttrs (version: info: {
  fileName = builtins.elemAt info 0;
  fileHash = builtins.elemAt info 1;
}) codexUpdateData.remarkable2
