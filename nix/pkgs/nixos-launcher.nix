{ replaceVarsWith, rm2-stuff }:
let
  rm2Pkgs = rm2-stuff.dev-rm2-toolchain;
in
replaceVarsWith {
  src = ./nixos-launcher.sh;
  replacements = {
    rm2fb-server = rm2Pkgs + "/bin/rm2fb_server";
    rm2fb-client = rm2Pkgs + "/lib/librm2fb_client_no_hook.so";
    yaft_reader = rm2Pkgs + "/bin/yaft_reader";
  };

  isExecutable = true;
}
