{ pkgs ? import <nixpkgs> {} }:

with pkgs; let
  developEnv = (overrideCC stdenv gcc13);
  boost = boost182;
in
  mkShell.override { stdenv = developEnv; } {
    name = "developEnv";
    packages = [
      gdb
      cmake-format
    ];
    inputsFrom = [
      (callPackage ./default.nix { stdenv = developEnv; inherit boost;})
    ];
  }

