{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";

    pre-commit-hooks = {
      url = "github:cachix/git-hooks.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    pre-commit-hooks,
  }:
    flake-utils.lib.eachSystem [
      "aarch64-darwin"
      "aarch64-linux"
      "x86_64-darwin"
      "x86_64-linux"
    ]
    (system: let
      pkgs = nixpkgs.legacyPackages.${system};
    in rec {
      formatter = pkgs.alejandra;

      checks = let
        hooks = {
          alejandra.enable = true;
          clang-format.enable = true;
          check-merge-conflicts.enable = true;
          check-shebang-scripts-are-executable.enable = true;
          check-added-large-files.enable = true;
        };
      in {
        pre-commit-check = pre-commit-hooks.lib.${system}.run {
          inherit hooks;
          src = ./.;
        };
      };

      devShells.default = pkgs.mkShell {
        inherit (self.checks.${system}.pre-commit-check) shellHook;

        name = "vulkan_renderer";
        inputsFrom = pkgs.lib.attrsets.attrValues packages;
        packages = with pkgs;
          [
            hyperfine
            valgrind
            clang-tools
          ]
          ++ (pkgs.lib.optionals
            pkgs.stdenv.isLinux [linuxPackages_latest.perf]);
      };

      packages.default = pkgs.stdenv.mkDerivation {
        name = "vulkan_renderer";
        src = pkgs.lib.cleanSource ./.;

        buildInputs = with pkgs; [
          glfw
          cglm
          vulkan-tools
          vulkan-loader
          vulkan-headers
          vulkan-validation-layers
          gnumake
          shaderc
          gcc
          xorg.libXxf86vm
          xorg.libXi
          xorg.libXrandr
        ];

        installPhase = ''
          mkdir -p $out/bin
          install -D vulkan_renderer $out/bin/vulkan_renderer --mode 0755
        '';
      };
    });
}
