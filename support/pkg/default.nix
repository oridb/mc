{ pkgs ? (import <nixpkgs> {})
, stdenv ? pkgs.stdenv
, bison ? pkgs.bison
, binutils ? pkgs.binutils
, fetchurl ? pkgs.fetchurl
}:

stdenv.mkDerivation rec {
  name = "myrddin";

  src = ../..;

  buildInputs = [ bison binutils ];

  preBuild = ''
    make bootstrap
  '';

  postPatch = ''
    substituteInPlace "mbld/opts.myr" --replace '"ld"' '"${binutils}/bin/ld"'
    substituteInPlace "configure"     --replace '"ld"' '"${binutils}/bin/ld"'
    substituteInPlace "mbld/opts.myr" --replace '"as"' '"${binutils}/bin/as"'
    substituteInPlace "configure"     --replace '"as"' '"${binutils}/bin/as"'
  '';
}
