{
  symlinkJoin,
  makeWrapper,
  ...
}:
package:
symlinkJoin {
  name = "rm2fb-wrapped-${package.name}";

  paths = [ package ];
  nativeBuildInputs = [ makeWrapper ];

  postBuild = ''
    for prog in  $out/bin/*; do
      wrapProgram $prog \
        --set LD_PRELOAD /run/current-system/sw/lib/librm2fb_client.so
    done
  '';
}
