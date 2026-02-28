{
  lib,
}:
let
  codexUpdateData = lib.importJSON (
    builtins.fetchurl {
      url = "https://raw.githubusercontent.com/Jayy001/codexctl/refs/heads/main/data/version-ids.json";
      sha256 = "sha256:0vil23p406mj449vixp0mqc78qgmlcyxmw3fyscfzh6x30drxz29";
    }
  );
in
builtins.mapAttrs (version: info: {
  fileName = builtins.elemAt info 0;
  fileHash = builtins.elemAt info 1;
  isLatest = false;
}) codexUpdateData.remarkable2
// {
  "3.25.1.1" = {
    isLatest = true;
    fileName = "remarkable-production-image-3.25.1.1-rm2-public.swu";
    fileHash = "6334aaf5885bf4f5d73d53bac4bdd62d533882187d259200da7df97ab71d5d5e";
  };
}
