{
  inputs = {
    hyprland = {
      type = "github";
      owner = "hyprwm";
      repo = "Hyprland";
      ref = "v0.55.4";
    };

    nixpkgs.follows = "hyprland/nixpkgs";
  };

  outputs = {
    self,
    nixpkgs,
    hyprland,
    ...
  }: let
    withPkgsFor = fn:
      nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system:
        fn system (import nixpkgs {
          inherit system;
          overlays = [
            hyprland.overlays.hyprland-packages
            self.overlays.default
          ];
        }));
  in {
    packages = withPkgsFor (system: pkgs: rec {
      inherit (pkgs.hyprlandPlugins) hyprexpo;

      default = hyprexpo;
    });

    overlays = {
      default = self.overlays.hyprexpo;

      hyprexpo = final: prev: {
        hyprlandPlugins =
          (prev.hyprlandPlugins or {})
          // {
            hyprexpo = final.callPackage ./default.nix {};
          };
      };
    };

    devShells = withPkgsFor (system: pkgs: {
      default = pkgs.mkShell.override {inherit (pkgs.hyprland) stdenv;} {
        shellHook = ''
          meson setup build --reconfigure
          sed -e 's/c++23/c++2b/g' ./build/compile_commands.json > ./compile_commands.json
        '';
        name = "hyprexpo-shell";
        nativeBuildInputs = with pkgs; [meson pkg-config ninja];
        buildInputs = [pkgs.hyprland];
        inputsFrom = [
          pkgs.hyprland
          pkgs.hyprlandPlugins.hyprexpo
        ];
      };
    });

    formatter = withPkgsFor (_: pkgs: pkgs.alejandra);
  };
}
