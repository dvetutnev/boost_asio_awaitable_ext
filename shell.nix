{ pkgs ? import <nixpkgs> {} }:

with pkgs; let
  developEnv = (overrideCC stdenv gcc13);
  boost = enableDebugging (boost182.override {
      stdenv = developEnv;
      enableDebug = true;
  });
in
  mkShell.override { stdenv = developEnv; } {
    name = "developEnv";
    packages = [
      gdb
      cmake-format
    ];
    inputsFrom = [
      (callPackage ./default.nix {
          stdenv = developEnv;
          inherit boost;
      })
    ];
  }

