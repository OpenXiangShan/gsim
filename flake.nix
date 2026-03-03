{
  description = "GSIM: A Fast RTL Simulator for Large-Scale Designs";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
  };

  outputs = { self, nixpkgs, ... }:
    let
      systems = nixpkgs.lib.systems.flakeExposed;
      perSystem = nixpkgs.lib.genAttrs systems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          llvm = pkgs.llvmPackages_19;
          gsim = llvm.stdenv.mkDerivation {
            pname = "gsim";
            version = "1.0.0-dev"; # FIXME?
            src = self;

            strictDeps = true;
            enableParallelBuilding = true;
            dontConfigure = true;

            nativeBuildInputs = [
              pkgs.flex
              pkgs.bison
            ];
            buildInputs = [
              pkgs.flex
              pkgs.gmp
            ];

            makeFlags = [
              # gsim defaults to /bin/bash, which is not guaranteed in NixOS
              "SHELL=${llvm.stdenv.shell}"
              # override default build target to avoid running tests
              "build-gsim"
            ];

            installPhase = ''
              runHook preInstall
              install -Dm755 build/gsim/gsim $out/bin/gsim
              runHook postInstall
            '';

            meta = {
              description = "Fast RTL Simulator for Large-Scale Designs";
              homepage = "https://github.com/OpenXiangShan/gsim";
              platforms = pkgs.lib.platforms.linux;
            };
          };
        in
        {
          packages = {
            gsim = gsim;
            default = gsim;
          };
        }
      );
    in
    {
      packages = nixpkgs.lib.mapAttrs (_: v: v.packages) perSystem;
    };
}
