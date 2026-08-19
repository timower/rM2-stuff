# libghostty-vt (ghostty's embeddable terminal-state library), built via
# `zig build` -- see nix/overlays/ghostty-vt.nix for how it's wired up.
{
  lib,
  stdenv,
  zig,
  callPackage,
  fetchFromGitHub,
  # Override when stdenv.hostPlatform doesn't match the real target (e.g.
  # dev-rm2-toolchain's native-zig arm cross-compile).
  zigTarget ? null,
  # zig defaults -Dcpu to the *build machine's* native CPU, which can crash
  # elsewhere (e.g. AVX-512 on CI runners, nixpkgs#169461/#185644).
  zigCpu ? "baseline",
}:
let
  hostPlatform = stdenv.hostPlatform;

  zigArch =
    {
      x86_64 = "x86_64";
      aarch64 = "aarch64";
      armv7l = "arm";
      armv6l = "arm";
      i686 = "x86";
    }
    .${hostPlatform.parsed.cpu.name}
      or (throw "ghostty-vt.nix: no zig arch mapping for cpu '${hostPlatform.parsed.cpu.name}'");

  zigOs =
    {
      linux = "linux";
      darwin = "macos";
    }
    .${hostPlatform.parsed.kernel.name}
      or (throw "ghostty-vt.nix: no zig os mapping for kernel '${hostPlatform.parsed.kernel.name}'");

  zigAbiSuffix =
    {
      gnu = "-gnu";
      gnueabihf = "-gnueabihf";
      musl = "-musl";
    }
    .${hostPlatform.parsed.abi.name} or "";

  resolvedTarget = if zigTarget != null then zigTarget else "${zigArch}-${zigOs}${zigAbiSuffix}";

  targetFlags = [
    "-Dtarget=${resolvedTarget}"
    "-Dcpu=${zigCpu}"
  ];

  ghosttySrc = fetchFromGitHub {
    owner = "ghostty-org";
    repo = "ghostty";
    rev = "f64f4aca2c29b554d111b36c3d946a9bddd159ff";
    hash = "sha256-tzt9C99wsV0lriI6LtLDYMhnNbWNwIzmcWfRJStGhMs=";
  };

  # zon2nix-generated lock file turning zig's package-manager deps into
  # individually content-addressed fetches, avoiding network access in-build.
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
    zig build -Demit-lib-vt -Doptimize=ReleaseFast --system ${deps} ${lib.escapeShellArgs targetFlags} --prefix $out
    runHook postBuild
  '';

  dontInstall = true;
}
