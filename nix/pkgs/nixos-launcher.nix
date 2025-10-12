{
  lib,
  replaceVarsWith,
  rm2-stuff,
}:
let
  rm2-display = rm2-stuff.rm2display;
  rm2-yaft = rm2-stuff.yaft;
in
replaceVarsWith {
  src = ./nixos-launcher.sh;
  replacements = {
    rm2fb-server = lib.getExe' rm2-display "rm2fb_server";
    rm2fb-client = rm2-display + "/lib/librm2fb_client_no_hook.so";
    yaft_reader = lib.getExe' rm2-yaft "yaft_reader";
  };

  isExecutable = true;
}
