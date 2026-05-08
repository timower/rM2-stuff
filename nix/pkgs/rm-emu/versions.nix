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
  "3.27.1.0" = {
    isLatest = true;
    fileName = "remarkable-production-image-3.27.1.0-rm2-public.swu";
    fileHash = "40f4b69ff4b546a9177b44e0f1b00e0e7bdc1b9694398b00b4fbca6301181ac2";
  };
}
