{
  description = "runner";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };
  outputs = { self, nixpkgs }: {
    devShells = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ] (system: {
      default = nixpkgs.legacyPackages.${system}.mkShell {
        nativeBuildInputs = with nixpkgs.legacyPackages.${system}; [
          pkg-config
          gcc
          gnumake
        ];
        buildInputs = with nixpkgs.legacyPackages.${system}; [
          glib
        ];
      };
    });
  };
}
