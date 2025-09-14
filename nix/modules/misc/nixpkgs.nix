{
  lib,
  nixpkgs,
  config,
  ...
}:
let
  overlayType = lib.mkOptionType {
    name = "nixpkgs-overlay";
    description = "nixpkgs overlay";
    check = lib.isFunction;
    merge = lib.mergeOneOption;
  };

  pkgs = import nixpkgs {
    localSystem = config.nixpkgs.buildPlatform;
    crossSystem = config.nixpkgs.hostPlatform;
    overlays = config.nixpkgs.overlays;
  };

  pkgsArmv7l = import nixpkgs {
    localSystem = config.nixpkgs.buildPlatform;
    crossSystem = lib.systems.examples.armv7l-hf-multiplatform;
  };
in
{
  options.nixpkgs = {
    buildPlatform = lib.mkOption {
      type = lib.types.either lib.types.str lib.types.attrs;
      apply = lib.systems.elaborate;
      description = "Platform to build on";
    };

    hostPlatform = lib.mkOption {
      type = lib.types.either lib.types.str lib.types.attrs;
      apply = lib.systems.elaborate;

      default = lib.systems.examples.remarkable2;
      description = "Platform of the remarkable target system";
    };

    overlays = lib.mkOption {
      default = [ ];
      type = lib.types.listOf overlayType;
      description = ''
        List of overlays to apply to Nixpkgs.
      '';
    };
  };

  config = {
    _module.args = { inherit pkgs; };

    # TODO: Nix doesn't build for remarkable2, use armv7
    nixpkgs.overlays = [
      (final: prev: {
        nix = if prev.system == "armv7l-linux" then pkgsArmv7l.nix else prev.nix;
      })
    ];
  };
}
