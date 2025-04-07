{
  description = "C++ project using curl and nlohmann_json";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    pkgs = nixpkgs.legacyPackages.x86_64-linux;
  in {
    packages.x86_64-linux.default = pkgs.stdenv.mkDerivation {
      pname = "wart";
      version = "1.0";

      src = ./.;

      nativeBuildInputs = [pkgs.gcc];
      buildInputs = [pkgs.curl pkgs.nlohmann_json];

      CXXFLAGS = ["-O3" "-march=native" "-flto" "-std=c++20" "-DNDEBUG"];
      LDFLAGS = ["-flto" "-s"];

      buildPhase = ''
        g++ $CXXFLAGS -o wart wart.cc -lcurl -I${pkgs.nlohmann_json}/include $LDFLAGS
        strip wart
      '';

      installPhase = ''
        mkdir -p $out/bin
        mv wart $out/bin/
      '';
    };
  };
}
