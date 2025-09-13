{
  lib,
  config,
  pkgs,

  ...
}:
let
  scriptType =
    with lib;
    with lib.types;
    either str (submodule {
      options = {
        deps = mkOption {
          type = listOf str;
          default = [ ];
          description = "List of dependencies";
        };
        text = mkOption {
          type = lines;
          description = "Script";
        };
      };
    });

  makeFullScript =
    set:
    let
      setWithStrings = builtins.mapAttrs (_: v: if builtins.isString v then lib.noDepEntry v else v) set;
    in
    ''
      #!${pkgs.runtimeShell}
      systemConfig=@out@
      export PATH=${
        lib.makeBinPath [
          pkgs.coreutils
          pkgs.nix
        ]
      }

      ${lib.textClosureMap lib.id setWithStrings (builtins.attrNames setWithStrings)}

      ln -sfn "$(readlink -f "$systemConfig")" /run/current-system
    '';
in
{
  imports = [ ];

  options = {
    system.path = lib.mkOption {
      internal = true;
      description = "The final environment of the system";
    };

    system.build = lib.mkOption {
      internal = true;
      type = lib.types.lazyAttrsOf lib.types.unspecified;
      default = { };
      description = "Attribute set of derivation used to setup the system.";
    };

    environment.systemPackages = lib.mkOption {
      type = lib.types.listOf lib.types.package;
      default = [ ];
      example = lib.literalExpression "[ pkgs.firefox pkgs.thunderbird ]";
      description = "Packages to install.";
    };

    system.activationScripts = lib.mkOption {
      default = { };
      description = "Set of shell fragments with dependencies";

      type = lib.types.attrsOf scriptType;
      apply = set: set // { script = makeFullScript set; };
    };

  };

  config = {
    system.path = pkgs.buildEnv {
      name = "nix-remarkable";
      paths = config.environment.systemPackages;
    };

    system.activationScripts.nix-profile = ''
      systemPath=$(readlink $systemConfig/sw)
      nix profile remove nix-remarkable
      nix profile install $systemPath
      unset systemPath
    '';

    system.build.toplevel = pkgs.stdenvNoCC.mkDerivation {
      name = "remarkable-system";

      activationScript = config.system.activationScripts.script;

      buildCommand = ''
        mkdir -p $out

        ln -s ${config.system.path} $out/sw

        echo "$activationScript" > $out/activate
        substituteInPlace $out/activate --subst-var out
        chmod u+x $out/activate
      '';
    };
  };
}
