{
  lib,
}:
let
  codexUpdateData = lib.importJSON (
    builtins.fetchurl {
      url = "https://raw.githubusercontent.com/Jayy001/codexctl/1687439d75c35c60a3670af72eb572e515ccda95/data/version-ids.json";
      sha256 = "sha256:03vars6h354fw7f03gzqr5j8a78sy4g4cqcwz8lmnkwlvif17m1d";
    }
  );
in
builtins.mapAttrs (version: info: {
  fileName = builtins.elemAt info 0;
  fileHash = builtins.elemAt info 1;
  isLatest = false;
}) codexUpdateData.remarkable2
// {
  "latest" = {
    isLatest = true;
    fileName = "remarkable-production-image-latest-rm2-public.swu";
    fileHash = "sha256-KQJpEYUAFO+Ukw8686cSj163qrbwzsPZ2BkbTz1iI28=";
  };
}
