{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "aarch64-darwin"
        "aarch64-linux"
        "x86_64-darwin"
        "x86_64-linux"
      ];
      forAllSystems =
        f: nixpkgs.lib.genAttrs systems (system: f (nixpkgs.legacyPackages.${system}));
    in
    {
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            cargo
            rustc
            rust-analyzer
            gcc
            nodejs_22
            pkg-config
            openssl

            # Tauri (webkit2gtk) 运行/构建依赖
            gtk3
            webkitgtk_4_1
            libsoup_3
            glib
            librsvg
            gdk-pixbuf
            glib-networking
            at-spi2-core
            gsettings-desktop-schemas
          ];

          env.LIBCLANG_PATH = "${pkgs.llvmPackages.libclang.lib}/lib";

          # gcc wrapper 不把 glibc 库目录放进链接搜索路径，glib/webkit 的 GLIBC_2.4x
          # 符号在链接期被误判为未定义；显式补上该 -L。
          shellHook = ''
            export RUSTFLAGS="''${RUSTFLAGS:-} -C link-arg=-L${pkgs.glibc}/lib"
            # GTK/WebKit 需要 GSettings schemas 才能启动，否则 GLib-GIO-ERROR 崩溃；
            # FileChooser 对话框需 gtk3 的 org.gtk.Settings.FileChooser，故拼接三个 schema 目录
            export GSETTINGS_SCHEMA_DIR="$(ls -d ${pkgs.gtk3}/share/gsettings-schemas/*/glib-2.0/schemas):$(ls -d ${pkgs.glib}/share/gsettings-schemas/*/glib-2.0/schemas):${pkgs.gsettings-desktop-schemas}/share/gsettings-schemas/${pkgs.gsettings-desktop-schemas.name}/glib-2.0/schemas"
            export XDG_DATA_DIRS="${pkgs.gsettings-desktop-schemas}/share:''${XDG_DATA_DIRS:-}"
            # WebKitGTK 的 DMABUF 渲染在部分驱动/Bind 下花屏或黑屏，强制软件合成 + X11 后端
            export WEBKIT_DISABLE_DMABUF_RENDERER=1
            export GDK_BACKEND=x11
          '';
        };
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-rfc-style);
    };
}
