{
  lib,
  hyprland,
  hyprgraphics,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "hyprexpo";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;
  buildInputs = [hyprgraphics];

  meta = with lib; {
    homepage = "https://github.com/sandwichfarm/hyprexpo";
    description = "An enhanced Hyprland workspaces overview plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
