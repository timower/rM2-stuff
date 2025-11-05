{
  toolchain_root ? null,
  preset ? null,

  fetchzip,
  stdenv,
  sdl2-compat,
  systemdLibs,
  libevdev,
  clang,
  cmake,
  ninja,
  pkg-config,
  xxd,
  ncurses,
  wget,
  ctestCheckHook,

  lib,
  ...
}:
let
  isRMToolchain = toolchain_root != null;
  isCross = toolchain_root != null || stdenv.buildPlatform != stdenv.hostPlatform;
  inherit (stdenv.hostPlatform) isDarwin;

  frida_system = if isRMToolchain then "armv7l-linux" else stdenv.hostPlatform.system;
  frida_info =
    {
      "armv7l-linux" = {
        os = "linux";
        arch = "armhf";
        hash = "sha256-7m5DdOt9YNNuVHK8IXF6KANM4aP+Hk/hMo5EuLfUB+M=";
      };
      "x86_64-linux" = {
        os = "linux";
        arch = "x86_64";
        hash = "sha256-WT8RjRj+4SKK3hIhw6Te/GuIy134zk2CXvpLWOqOJcM=";
      };
      "aarch64-darwin" = {
        os = "macos";
        arch = "arm64";
        hash = "sha256-xUOB505JWKUyJ4MAv9hP85NSOL9PX4DDJGjGO5ydWBM=";
      };
    }
    ."${frida_system}";

  frida_gum = fetchzip {
    url = "https://github.com/frida/frida/releases/download/16.1.4/frida-gum-devkit-16.1.4-${frida_info.os}-${frida_info.arch}.tar.xz";
    inherit (frida_info) hash;
    stripRoot = false;
  };
  expected = fetchzip {
    url = "https://github.com/TartanLlama/expected/archive/refs/tags/v1.1.0.tar.gz";
    hash = "sha256-AuRU8VI5l7Th9fJ5jIc/6mPm0Vqbbt6rY8QCCNDOU50=";
  };
  catch2 = fetchzip {
    url = "https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.tar.gz";
    hash = "sha256-DqGGfNjKPW9HFJrX9arFHyNYjB61uoL6NabZatTWrr0=";
  };
  utfcpp = fetchzip {
    url = "https://github.com/nemtrif/utfcpp/archive/refs/tags/v4.0.0.tar.gz";
    hash = "sha256-IuFbWS5rV3Bdmi+CqUrmgd7zTmWkpQksRpCrhT499fM=";
  };

  libevdev-static = libevdev.overrideAttrs (old: {
    dontDisableStatic = true;
  });

  getCompomentOut = comp: builtins.replaceStrings [ "-" ] [ "_" ] comp;
  components = [
    # TODO: "rMlib", fails to copy includes
    "rocket"
    "tilem"
    "yaft"
    "tools"
  ]
  ++ lib.optionals (!isDarwin) [
    "ioctl-dump"
    "rm2display"
  ];
in
stdenv.mkDerivation {
  pname = "rM2-stuff";
  version = "master";

  # src = ./../..;
  src = builtins.path {
    path = ./../..;
    name = "rm2-stuff-src";
    filter =
      path: type:
      (type != "regular" || builtins.baseNameOf path != "flake.nix")
      && (type != "directory" || builtins.baseNameOf path != "nix");
  };

  buildInputs =
    lib.optionals (!isCross) [
      sdl2-compat.dev
    ]
    ++ lib.optionals (!isRMToolchain && !isDarwin) [
      systemdLibs.dev
      libevdev-static
    ];

  nativeBuildInputs =
    lib.optionals (!isCross) [
      clang
      ninja
      ctestCheckHook
    ]
    ++ lib.optionals (!isRMToolchain) [
      pkg-config
    ]
    ++ [
      xxd
      cmake
      ncurses
    ];

  dontFixCmake = isRMToolchain;

  TOOLCHAIN_ROOT = lib.optionalString isRMToolchain "${toolchain_root}";

  cmakeFlags = [
    "-DFETCHCONTENT_SOURCE_DIR_FRIDA-GUM=${frida_gum}"
    "-DFETCHCONTENT_SOURCE_DIR_EXPECTED=${expected}"
    "-DFETCHCONTENT_SOURCE_DIR_CATCH2=${catch2}"
    "-DFETCHCONTENT_SOURCE_DIR_UTFCPP=${utfcpp}"
  ]
  ++ lib.optionals isRMToolchain [
    "-DCMAKE_TOOLCHAIN_FILE=${./../../cmake/rm-toolchain.cmake}"
  ]
  ++ lib.optionals (!isCross) [
    "-DEMULATE=ON"
    "-DBUILD_TESTS=ON"
  ]
  ++ lib.optionals (preset != null) [
    "--preset=${preset}"
    "-B."
  ];

  prePatch = ''
    substituteInPlace apps/tilem/Dialog.cpp \
      --replace-fail 'wget' ${lib.getExe wget}
  '';

  outputs = [ "out" ] ++ builtins.map getCompomentOut components;
  installPhase = ''
    runHook preInstall

    cmake --install . --prefix $out

    ${lib.strings.concatMapStringsSep ";" (
      comp: "cmake --install . --component ${comp} --prefix ${placeholder (getCompomentOut comp)}"
    ) components}

    runHook postInstall
  '';

  doCheck = !isCross;
  # nativeCheckInputs = [ wget ];
}
