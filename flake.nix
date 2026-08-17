{
  description = "GSIM: A Fast RTL Simulator for Large-Scale Designs";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs, ... }:
    let
      systems = nixpkgs.lib.systems.flakeExposed;
      version = self.rev or self.dirtyRev or "UNKNOWN";
      perSystem = nixpkgs.lib.genAttrs systems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          llvm = pkgs.llvmPackages;
          gsim = llvm.stdenv.mkDerivation {
            pname = "gsim";
            version = version;
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
              pkgs.jemalloc
            ];

            makeFlags = [
              # gsim defaults to /bin/bash, which is not guaranteed in NixOS
              "SHELL=${llvm.stdenv.shell}"
              # nix builder does not have access to .git, so 'git describe' will fail, we need pass this explicitly
              "GSIM_VERSION=${version}"
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
            gsim-static = (gsim.overrideAttrs (prev: {
              pname = "${prev.pname}-static";
              buildInputs = prev.buildInputs ++ [
                pkgs.pkgsStatic.gmp
                pkgs.pkgsStatic.jemalloc
                llvm.stdenv.cc.libc.static
              ];
              makeFlags = prev.makeFlags ++ [
                "STATIC=1"
              ];
            }));
            default = gsim;
          };
        }
      );
    in
    {
      packages = nixpkgs.lib.mapAttrs (_: v: v.packages) perSystem;
    };
}
