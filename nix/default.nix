{
  preset ? "dev-host",
  toolchain_root ? null,

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

  lib,
  ...
}:
let
  isCross = toolchain_root != null;

  # TODO: base on host arch.
  frida_arch = if isCross then "armhf" else "x86_64";
  frida_hash =
    if isCross then
      "sha256-7m5DdOt9YNNuVHK8IXF6KANM4aP+Hk/hMo5EuLfUB+M="
    else
      "sha256-WT8RjRj+4SKK3hIhw6Te/GuIy134zk2CXvpLWOqOJcM=";

  frida_gum = fetchzip {
    url = "https://github.com/frida/frida/releases/download/16.1.4/frida-gum-devkit-16.1.4-linux-${frida_arch}.tar.xz";
    hash = frida_hash;
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
in
stdenv.mkDerivation {
  pname = "rM2-stuff";
  version = "master";
  src = ./..;

  buildInputs = lib.optionals (!isCross) [
    sdl2-compat.dev
    systemdLibs.dev
    libevdev
  ];

  nativeBuildInputs =
    lib.optionals (!isCross) [
      clang
      pkg-config
      xxd
    ]
    ++ [
      cmake
      ninja
      ncurses
    ];

  dontFixCmake = isCross;

  TOOLCHAIN_ROOT = lib.optionalString (toolchain_root != null) "${toolchain_root}";

  cmakeFlags = [
    "-DFETCHCONTENT_SOURCE_DIR_FRIDA-GUM=${frida_gum}"
    "-DFETCHCONTENT_SOURCE_DIR_EXPECTED=${expected}"
    "-DFETCHCONTENT_SOURCE_DIR_CATCH2=${catch2}"
    "-DFETCHCONTENT_SOURCE_DIR_UTFCPP=${utfcpp}"
    "--preset=${preset}"
    "-B."
  ];
}
