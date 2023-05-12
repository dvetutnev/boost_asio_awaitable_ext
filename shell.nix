{ pkgs ? import <nixpkgs> {} }:

let
  stdenv = pkgs.gcc12Stdenv;
  boost = pkgs.boost182;
in
  pkgs.mkShell {
    name = "nix-shell";
    packages = with pkgs; [
      gdb
      clang-tools_16
      cmake-format
    ];
    inputsFrom = [
      (pkgs.callPackage ./default.nix { inherit stdenv boost; })
    ];
  }

