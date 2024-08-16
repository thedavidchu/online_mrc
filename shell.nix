let
  nixpkgs = fetchTarball "https://github.com/NixOS/nixpkgs/tarball/nixos-24.05";
  pkgs = import nixpkgs { config = {}; overlays = []; };
in

pkgs.mkShellNoCC {
  packages = with pkgs; [
    # Packages are listed here: https://search.nixos.org/packages
    stdenvNoCC
    git
    # NOTE  I'm not sure why 'libgcc' doesn't work.
    gcc
    pkg-config
    glib
    glibc
    boost
    xsimd
    python312
    python312Packages.meson
    python312Packages.ninja
    python312Packages.numpy
    python312Packages.matplotlib
    python312Packages.tqdm
    llvmPackages_12.clang-unwrapped # clang-format, clangd
    llvmPackages_12.bintools
    valgrind
    which
  ];

  CC = "x86_64-unknown-linux-gnu-gcc-13.2.0";
  shellHook = "meson setup build && cd build && meson compile";
}