{
  lib,
  pkgs,
  config,
  ...
}:
let
  # `pkgsi686Linux` reached through any accessor chained off a cross `pkgs`
  # (`buildPackages`, `pkgsBuildBuild`, ...) resolves to a binary built *for
  # the armv7l target*, not one runnable on the build machine — a nixpkgs
  # quirk, not something fixable by picking a different accessor. Build an
  # independent, non-cross nixpkgs evaluation for the build platform instead.
  buildPkgs = import pkgs.path { system = pkgs.stdenv.buildPlatform.system; };

  # LuaJIT's build needs a real 32-bit x86 host compiler (`pkgsi686Linux`),
  # only available when building from an x86_64 build machine — same
  # constraint as the `koreader` package output in flake.nix.
  buildSupported = pkgs.stdenv.buildPlatform.isx86_64;

  koreader = pkgs.callPackage ../pkgs/koreader.nix {
    inherit (buildPkgs) pkgsi686Linux;
  };
in
{
  options.programs.koreader = {
    enable = lib.mkEnableOption "KOReader";
  };

  config = lib.mkIf config.programs.koreader.enable (
    lib.mkMerge [
      (lib.mkIf buildSupported {
        environment = {
          systemPackages = [ koreader ];
          etc."draft/koreader.draft".text = ''
            name=KOReader
            desc=An ebook reader application supporting PDF, DjVu, EPUB, FB2 and many more formats
            call=${lib.getExe koreader}
            term=:
            imgFile=koreader
          '';
          etc."draft/icons/koreader.png".source = "${koreader}/koreader/icon.png";
        };
      })
      (lib.mkIf (!buildSupported) {
        warnings = [
          "programs.koreader.enable is set, but cross-compiling KOReader requires an x86_64 build machine (LuaJIT needs a 32-bit x86 host compiler) — skipping"
        ];
      })
    ]
  );

}
