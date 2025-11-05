_:
{
  nixpkgs.overlays = [
    (final: prev: {
      rm2-stuff = final.callPackage ../pkgs/rm2-stuff.nix { };
    })
  ];
}
