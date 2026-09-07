import { defineConfig } from "vitepress";

export default defineConfig({
  title: "HyprExpo",
  description: "Expose-style workspace overview plugin for Hyprland",
  base: "/docs/",
  outDir: "../dist/docs",
  cleanUrls: true,
  head: [
    ["meta", { name: "theme-color", content: "#07090f" }],
    ["link", { rel: "icon", type: "image/svg+xml", href: "/docs/hyprexpo-mark.svg" }],
  ],
  themeConfig: {
    nav: [
      { text: "Docs", link: "/" },
      { text: "Install", link: "/getting-started/installation" },
      { text: "Config", link: "/configuration/options" },
      { text: "GitHub", link: "https://github.com/sandwichfarm/hyprexpo" },
    ],
    sidebar: [
      {
        text: "Getting Started",
        items: [
          { text: "Overview", link: "/" },
          { text: "Installation", link: "/getting-started/installation" },
          { text: "Quick Start", link: "/getting-started/quick-start" },
        ],
      },
      {
        text: "Configuration",
        items: [
          { text: "Options", link: "/configuration/options" },
          { text: "Labels and Borders", link: "/configuration/labels-borders" },
          { text: "Keyboard Navigation", link: "/configuration/keyboard" },
        ],
      },
      {
        text: "Guides",
        items: [
          { text: "Scrolling Overview", link: "/guides/scrolling-overview" },
          { text: "Lua Gestures", link: "/guides/lua-gestures" },
          { text: "Multi-Monitor Layouts", link: "/guides/multi-monitor" },
          { text: "Migration", link: "/guides/migration" },
          { text: "Runtime Smoke", link: "/guides/runtime-smoke" },
          { text: "Release Promotion", link: "/guides/release-promotion" },
          { text: "Development Installation", link: "/guides/development-installation" },
        ],
      },
      {
        text: "Reference",
        items: [
          { text: "Dispatchers", link: "/reference/dispatchers" },
          { text: "Compatibility", link: "/reference/compatibility" },
          { text: "Branches and Releases", link: "/reference/branch-policy" },
          { text: "Workflow Validation", link: "/reference/workflow-validation" },
          { text: "Troubleshooting", link: "/troubleshooting" },
        ],
      },
    ],
    search: {
      provider: "local",
    },
    socialLinks: [
      { icon: "github", link: "https://github.com/sandwichfarm/hyprexpo" },
    ],
    footer: {
      message: "HyprExpo is a maintained Hyprland plugin fork.",
      copyright: "MIT Licensed",
    },
  },
});
