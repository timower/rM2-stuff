{
  lib,
  pkgs,
  config,
  rm2-stuff,
  ...
}:
{
  options = {
    rm2display = {
      enable = lib.mkOption {
        type = lib.types.bool;
        default = true;
        description = "Enable rm2-stuff rm2fb display server";
      };
    };

    programs.yaft = {
      enable = lib.mkOption {
        type = lib.types.bool;
        default = false;
        description = "Enable rm2-stuff yaft";
      };
    };

    programs.tilem = {
      enable = lib.mkOption {
        type = lib.types.bool;
        default = false;
        description = "Enable rm2-stuff tilem";
      };
    };

    programs.rocket = {
      enable = lib.mkOption {
        type = lib.types.bool;
        default = false;
        description = "Enable rm2-stuff rocket launcher";
      };
    };
  };

  config = {
    nixpkgs.overlays = lib.mkMerge [
      [
        (final: prev: {
          rm2-stuff = rm2-stuff.dev-cross;
        })
      ]
      (lib.mkIf config.rm2display.enable [
        (final: prev: {
          # Use rm2-toolchain for the server implementation.
          rm2display = rm2-stuff.dev-rm2-toolchain.rm2display;

          wrapWithClient = final.callPackage ../../pkgs/wrapWithClient.nix { };
        })
      ])
    ];

    environment.systemPackages = lib.mkMerge [
      (lib.mkIf config.rm2display.enable [
        pkgs.rm2display
      ])
      (lib.mkIf config.programs.yaft.enable [
        (pkgs.wrapWithClient pkgs.rm2-stuff.yaft)
      ])
      (lib.mkIf config.programs.tilem.enable [
        (pkgs.wrapWithClient pkgs.rm2-stuff.tilem)
      ])
      (lib.mkIf config.programs.rocket.enable [
        (pkgs.wrapWithClient pkgs.rm2-stuff.rocket)
      ])
    ];
  };
}
