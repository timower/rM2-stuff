{
  runCommand,
  buildPackages,
  nix,
  cacert,

  extraPaths ? [ ],
}:
let
  installerClosureInfo = buildPackages.closureInfo {
    rootPaths = [
      nix
      cacert
    ]
    ++ extraPaths;
  };
in
runCommand "nix-installer" { } ''
  cp ${installerClosureInfo}/registration $TMPDIR/reginfo

  substitute ${./nix-installer.sh} $TMPDIR/install \
    --subst-var-by nix ${nix} \
    --subst-var-by cacert ${cacert}
  chmod +x $TMPDIR/install

  dir=nix-installer
  fn=$out/$dir.tar.xz

  mkdir -p $out/nix-support
  echo "file binary-dist $fn" >> $out/nix-support/hydra-build-products

  tar cfJ $fn \
    --owner=0 --group=0 --mode=u+rw,uga+r \
    --mtime='1970-01-01' \
    --absolute-names \
    --hard-dereference \
    --transform "s,$TMPDIR/install,$dir/install," \
    --transform "s,$TMPDIR/reginfo,$dir/.reginfo," \
    --transform "s,$NIX_STORE,$dir/store,S" \
    $TMPDIR/install \
    $TMPDIR/reginfo \
    $(cat ${installerClosureInfo}/store-paths)
''
