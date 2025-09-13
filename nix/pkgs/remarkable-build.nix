{
  lib,
  runtimeShell,
  replaceVarsWith,
  jq,
  nix,
  openssh,
  nix-installer,
}:
let
  path = lib.makeBinPath [
    jq
    openssh
    nix
  ];
in
replaceVarsWith {
  src = ./remarkable-build.sh;
  replacements = {
    inherit runtimeShell path;
    nix_installer = nix-installer;
  };

  name = "remarkable-build";
  dir = "bin";
  isExecutable = true;
  meta.mainProgram = "remarkable-build";
}
