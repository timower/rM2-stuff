{
  symlinkJoin,
  makeWrapper,
  rm2-stuff,
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
        --set LD_PRELOAD ${rm2-stuff.rm2display}/lib/librm2fb_client.so
    done
  '';
}
