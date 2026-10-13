# AD/DA 生产烧录校准（产线流水）

Tauri 2 + React + TS + Vite + Tailwind v4。产线自动化：烧录 DUT 校准固件 → 自动校准 → 复检 PASS 后烧录 APP 固件 → 校验 APP 启动日志。流程结束后点「重新运行」即可对下一台板重复整条流水。

## 流程（单台）

1. **烧录校准固件（DUT）**：probe-rs 烧录用户选定的 DUT ELF/HEX。
2. **附着校准固件 RTT**：`Rtt::attach` 扫内存定位控制块（固件 `-s` 去符号）。
3. **自动校准**：下发 CONFIG + START 帧，读上行日志喂解析器，直到复检结论 `calib: verify PASS/FAIL`。
   - **仅 PASS 才进入下一步**；FAIL / 超时 / 连接失败 → 停止，不烧 APP（核心不变式）。
4. **烧录 APP 固件**：PASS 后烧录用户选定的 APP ELF/HEX。
5. **附着 APP 固件 + 校验启动**：10 s 内收集启动日志，必须同时出现 `Device: <板型>, App Version: vX.Y`、`Init success`，以及本板标定记录加载成功标记 `calib ad load ok mask=…` / `calib da load ok mask=…`（APP 在 `BoardInit()` 触发一次换算时打印，`mask` 须非 0；失败为 `calib <board> load fail (raw passthrough)`）才算 PASS。板型取自校准阶段日志的 `calib: board ad|da`，标记必须与板型匹配；该校验同时证明烧 APP 后校准扇区未被破坏。

## 校准扇区保护

烧录一律用 `DownloadOptions { verify:true, do_chip_erase:false, keep_unwritten_bytes:true }`：
- 按扇区擦除而非整片擦除，不动 `0x0800F000` 校准记录。
- `keep_unwritten_bytes` 擦后回读恢复未写字节，校准记录在扇区内也保留。
- DUT→APP 切换不清除刚校准写入的系数。

## 开发

```bash
nix develop                 # 或 direnv allow，提供 rust/node/webkit 依赖
npm install
npm run tauri dev           # 启动桌面应用
npm run build               # 前端 tsc + vite 构建
(cd src-tauri && cargo test) # 纯函数单测（verify_log_pass / calib_pass 等）
```

## 说明

- REF 板是产线夹具，本工具只处理 DUT，从不烧录/校验 REF。
- 探针由 probe-rs 独占：运行期间不要同时执行固件仓库的 `just run` / `just attach`。
- 单台流程结束（PASS 或失败）后，主按钮显示「重新运行」，点它即从头重复整条流水；上一轮线程与探针会在重新启动时回收后重开。
- 流程线程退出即把顶栏连接状态置为「已断开」（探针随会话释放，UI 不再常驻「已连接」）；下一台板可直接点「重新运行」，「连接」自检非必需。
- 无硬件验证：`cargo test` 覆盖纯函数逻辑（日志判据、校准 PASS/FAIL 判定），烧录/附着/RTT 需两块板 + 探针在环验证。
- `prod/` 是嵌套在固件仓库里的 flake；`nix develop` 只认 git 已跟踪的文件，未 `git add` 时 `prod/flake.nix` 对 nix 不可见，需先 `git add prod/`。
- devShell 的 `shellHook` 会给 `RUSTFLAGS` 追加一条 `-L<glibc>/lib`：本机 nix gcc wrapper 未把 glibc 目录放入链接搜索路径，缺它则链接 tauri 依赖时报 `undefined reference to free_sized@GLIBC_2.43`。
- 发布（bundle）已停用（`bundle.active=false`）：`generate_context!` 不再要求打包图标，产线用开发构建即可。
