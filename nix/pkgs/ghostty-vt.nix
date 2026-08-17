# libghostty-vt (ghostty's embeddable terminal-state library), built via
# `zig build`. Call with pkgs.callPackage for the host and
# pkgsArmv7.callPackage for the rM2 cross target -- see flake.nix, which
# overrides `zig` on both calls (ghostty requires zig 0.16.0, newer than the
# repo's pinned nixpkgs, so the caller overlays it in via nixpkgs' own
# zig.override({version, hash, llvmPackages})).
{
  lib,
  stdenv,
  zig,
  callPackage,
  fetchFromGitHub,
  zigTargetFlags ? [ ],
}:
let
  ghosttySrc = fetchFromGitHub {
    owner = "ghostty-org";
    repo = "ghostty";
    rev = "f64f4aca2c29b554d111b36c3d946a9bddd159ff";
    hash = "sha256-tzt9C99wsV0lriI6LtLDYMhnNbWNwIzmcWfRJStGhMs=";
  };

  # `zig build` fetches its package-manager deps (e.g. `uucode`) over the
  # network, which a sandboxed derivation can't do. ghostty's own repo
  # vendors a zon2nix-generated lock file (build.zig.zon.nix) at every
  # commit -- the same mechanism nixpkgs' own `ghostty` package uses --
  # turning each dependency into an individually content-addressed fetch.
  # `callPackage` is auto-injected by whichever pkgs set built this
  # derivation (host or pkgsArmv7); that's fine unmodified -- fetchers like
  # fetchurl/fetchgit are always the native/build-platform ones in nixpkgs
  # regardless of which package set's callPackage supplied them.
  deps = callPackage "${ghosttySrc}/build.zig.zon.nix" {
    zig_0_16 = zig;
    name = "ghostty-vt-deps";
  };
in
stdenv.mkDerivation {
  pname = "ghostty-vt";
  version = "0-unstable-2026-08-17";
  src = ghosttySrc;

  nativeBuildInputs = [ zig ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    export HOME=$TMPDIR
    export ZIG_GLOBAL_CACHE_DIR=$TMPDIR/zig-cache
    zig build -Demit-lib-vt -Doptimize=ReleaseFast --system ${deps} ${lib.escapeShellArgs zigTargetFlags} --prefix $out
    runHook postBuild
  '';

  dontInstall = true;
}
