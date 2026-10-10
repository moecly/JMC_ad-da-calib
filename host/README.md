# AD/DA 校准上位机（RTT 只读显示）

Tauri 2 + React + TS + Vite + Tailwind v4。通过 SWD/RTT 只读观察 DUT 固件（`coupler-module-aio`）的校准日志，解析成表格与误差曲线。本轮不做下行控制。

## 开发

```bash
nix develop                 # 或 direnv allow，提供 rust/node/webkit 依赖
npm install
npm run tauri dev           # 启动桌面应用
npm run build               # 前端 tsc + vite 构建
(cd src-tauri && cargo test) # 解析器单测
```

## 说明

- 探针由 probe-rs 独占：运行期间不要同时执行固件仓库的 `just run` / `just attach`。
- RTT 只有 1 个上行通道（`log`）；`Rtt::attach` 扫内存定位控制块，不依赖 ELF 符号（固件 `-s` 去符号）。
- 无硬件时可用界面 `加载日志` 回放 `just run … | tee dut.log` 保存的日志。
- 若 `flake.nix` 中某包名（如 `at-spi2-core` / `webkitgtk_4_1`）在当前 nixpkgs 缺失，用 `nix-shell -p` 等价包名替代。
- `host/` 是嵌套在固件仓库里的 flake；`nix develop` 只认 git 已跟踪的文件，未 `git add` 时 `host/flake.nix` 对 nix 不可见，需先 `git add host/`。
- devShell 的 `shellHook` 会给 `RUSTFLAGS` 追加一条 `-L<glibc>/lib`：本机 nix gcc wrapper 未把 glibc 目录放入链接搜索路径，缺它则链接 tauri 依赖时报 `undefined reference to free_sized@GLIBC_2.43`。
