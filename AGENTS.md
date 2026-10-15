# Repository Guidelines

AT32F425K8T7（Cortex-M4，裸机）多板型工业耦合器模块固件。单仓库同时承载多个硬件板型，通过 CMake 变量在编译期选择板型；应用框架（`user/`）与板级实现（`Board/<BOARD>/`）分层解耦。C/C++ 混编（C99 + C++17）。

## Project Overview

- 目标芯片：AT32F425K8T7，FLASH 64K @ `0x08000000`，RAM 20K @ `0x20000000`（见 `ldscripts/AT32F425x8_FLASH.ld`）。应用区限制为 60K（`LENGTH = 60K`），末扇区 `0x0800F000`（2 KB）留给校准系数存储，格式见 `docs/calibration-flash.md`。
- 一类板型一个模块：AD 采集（`AD_4CH`，AD7689 SPI ADC）、DA 输出（`DA_4CH`，MS5542 SPI DAC）。
- 当前状态：`AD_4CH` / `DA_4CH` 驱动与标定在调试中，`Board::HandleData()` 内的数据通路大多被注释；早期模块通信协议（`ModuleComm`）已移除，仅残留于 `user/InterruptCallback.cpp.bak`。

## Architecture & Data Flow

**分层**

1. `firmware/` + `include/libraries/` — AT32 标准外设库 v2（原厂，勿改）。编为静态库 `libat32.a`。
2. `cmsis/` — 启动汇编与 `SystemInit`，编入 app。
3. `Board/<BOARD>/` — 板级实现：`Board.h/.cpp` + 传感器/执行器驱动（`AD7689Drv`、`MS5542`）+ AT32 Workbench 生成的 `project/`。
4. `user/` — 与板型无关的应用框架与通用组件。
5. `calib/` — 校准，仅 `-DCALIB_ROLE=REF|DUT|MANUAL` 时编入 app：`CalibProc` 状态机（三分支：`REF` 应答、`MANUAL` 单板手动校准、`DUT` 自动双板流程）、`CalibOps.h`（RTT 下行/RS485 协议常量）、`CalibLink`（USART2 RS485）、`CalibStore`（Flash 记录 v2）、`PolyFit`（最小二乘三次拟合）。DUT 默认上电等待上位机经 RTT 下行启动（`CALIB_AUTO_START` 编译开关可恢复自动流程），支持按「连线」选择单通道校准；启动前经 RS485 查询 REF 全量信息（`OP_GET_INFO`）。`MANUAL` 为单板、无 REF 的手动校准，不用 RS485。门限与采样节拍自 v1.9 起为**运行时参数**（上位机经 `OP_HOST_PARAMS` 下发，不落 Flash），其中 `skip_verify` 允许落盘后跳过复检、直接判 `S_DONE`。拟合残差/复检超差时**只重测超标点**（`retry_count` 默认 3，含首测）：重测值仅在与当前拟合（复检用落盘系数）更贴合时才取代旧样本，故一轮重测不会把原本合格的通道拖坏；轮数用尽仍超标才判失败。

**两处多态**

- 编译期：`-DCONFIG_DEVICE_<BOARD>` → `user/FWConfig.h` 展开 `CONFIG_DEVICE_NAME` / `MODULE_TYPE`。新增板型需在 `FWConfig.h` 增加分支。另有 `-DCALIB_ROLE=APP|REF|DUT|MANUAL`（默认 `APP`）选择校准角色；非 `APP` 时定义 `CONFIG_CALIB_ROLE_<ROLE>`、编入 `calib/` 源文件，并在产物名追加 `-<ROLE>`。
- 运行期：`user/BaseBoard.h` 是纯虚接口（`BoardInit()` / `HandleData()` / `OneMsTask()`）；`Board` 类由各板型目录的 `Board.h` 提供；`user/Application.h` 的 `Application : public Board`（Meyer's 单例）在 **应用目标** 里继承它。构建系统为 app 目标加入 `Board/${BOARD}` 作为 include 路径，因此 `#include "Board.h"` 随 `-DBOARD` 解析到不同实现——这是板型选择的关键机制。

**启动与数据流**

```
main.c (Board/<BOARD>/project/src/)
  → at32f425_wk_config.c 外设初始化（Workbench 生成）
  → CPPInterface.h: ApplicationInit()   [extern "C"]
     → Application::Start() → Init(): ClockInit(96MHz) / PeriphInit / PinInit / TmrInit / BoardInit()
     → 主循环: SoftTimerLoop() + HandleData()
     → SerialOutStream::Loop() → USART4

TMR1 1ms 中断 → InterruptCallback → AppOneMsTask() → Application::OneMsTask()
             → SoftTimerTick(tick) → 软定时器链表 → Board::OneMsTask()
```

- 采样链路（AD）：`Board::OneMsTask() → AD7689Drv::Sync()`（SPI1 读取 + 滑动平均，深度 `AD7689Drv::FILTER_NUM`，每 1ms 轮转一路、4ms 一轮四路）→ `HandleData()` 取 `GetAdData()` → 输出。`FILTER_NUM` 越大越稳但滞后越久，改它会同时移动标定所需的建压等待 `kSettleMs`。
- 输出链路（DA）：`HandleData() → MS5542::SetVal()`（SPI1 写入，四路独立 CS）。
- 日志：`LogInfo << "x = " << x;` → `CuteLogger` RAII 临时对象析构时提交 → `Logger` 单例 → `SerialOutStream` 环形缓冲（10×200B）→ 主循环非阻塞发送到 USART4。中断内**不得**直接阻塞写串口，只入队。单条格式 `[LEVEL] 文件名:行号 (函数): 内容`（`Logger::FormatData` 只取 basename，不打印完整路径）。
- 板级适配点：`CalibAdIdeal()` / `CalibDaRaw()` 以 Flash 校准记录系数为准；无有效记录（或该通道 `validMask` 未置位）时按原值直通（AD 侧 `ideal=raw`、DA 侧 `raw=ideal`），即不做换算。硬编码内建曲线已移除——它是单体级标定产物，换板不适用，新板一律经手动校准写入记录。

## Key Directories

| 路径 | 用途 |
| --- | --- |
| `user/` | 应用框架：`Application`、`BaseBoard`、`CPPInterface`、`DrvInit`、`BaseTimer`、`SerialOutStream`、`FWConfig.h`、`Clock.c` |
| `user/{CRC,CuteLogger,Delay,DigitalInFilter,Led,SoftTimer,Timer}/` | 通用组件：日志、软定时器、延时抽象、数字滤波、LED、CRC |
| `calib/` | 校准，仅 `CALIB_ROLE` 非 `APP` 时编入：`CalibProc`（状态机/拟合/落盘/写盘后复检/dump 导出 + 上位机下行控制）、`CalibOps.h`（opcode 常量 + 手动读写模式常量 `OP_MODE_RAW/CAL`）、`CalibLink`（USART2 RS485 收发热链路）、`CalibStore`（`0x0800F000` 记录 v2 读写 + CRC-32）、`PolyFit`（三次多项式最小二乘）。状态机：`S_IDLE`（默认，等待上位机 start）→ `S_PING`→`S_SWEEP`→`S_FIT`→`S_DUMP`→`S_SAVE`→`S_VERIFY`→`S_DONE`，失败 `S_FAIL`；`CALIB_AUTO_START` 令 DUT 上电直接 `S_PING`（mask=0x0F，REF ch0）。上位机经 RTT 下行 `OP_HOST_*` 帧指定待校准通道位掩码与每通道 REF 源，**单次 67 点扫描并行标定全部选中通道**（每个勾选通道各一个源→通道映射，逐点对所有去重后的 REF 驱动源同时施加、一次读回全部通道，`FitAll` 一次拟合全部；`calib: run ch= da=` 每通道各一行）；支持暂停/继续/重启/停止，手动写码仅非运行态。角色分支：`REF` 应答 DUT 的 `OP_GET_INFO`（返回板型/版本/是否已有校准记录/`validMask`）；`MANUAL` 为单板手动校准（RTT 下行 `OP_HOST_MANUAL_*`：begin/abort/set-raw/add/fit/save），AD 板逐点采集（`mpt`）、DA 板写 raw 后录实测值，拟合 `mres`、落盘 `msaved`、回读复检 `mverify`，不用 RS485。每次校准只回写被标定通道的系数位（`CalibStoreLoad` 合并保留其余系数），落盘 `validMask`。每点采集 = 等 `s_settleMs` 让 DA 建压，再按 `s_sampleIntervalMs` 采 `s_stableTarget` 次，窗内取中值（`MedianWin`，抑制单次毛刺）；窗内极差 > `s_stableCode` 仅打 `unstable` 标记，不中断。`FitAll` 对每个选中通道打印 `ch%d resid max=%.1f @ideal=%u rmse=%.1f`，任一超 `s_residualLimit` 时列出全部超标点并 `EnterFail()`。常量均在 `CalibProc.cpp` 顶部——其中门限与采样节拍自 v1.9 起为**运行时参数**（`kDef*` 为默认初值，实际用 `s_*`；上位机经 `OP_HOST_PARAMS` 下发，运行中改回 `calib: error busy`）。**两方向均支持**：AD 板 DUT + DA 板 REF（DUT 发 `OP_SET_DA` 给 REF、回读本机 AD）；DA 板 DUT + AD 板 REF（DUT 本机写 DA raw、REF 回读 AD ideal，扫描逐点 `calib: dpt…`、复检逐点 `calib: dvfy…`、手动读参考 `calib: refad…`）。`S_PING` 收到 `OP_PONG` 后查询 REF 全量信息并校验板型方向：未应答 `calib: error ref no-reply`、板型不符 `calib: error ref mismatch`，两者均落 `S_FAIL`。详见 Testing & QA |
| `docs/` | 设计文档：`calibration-flash.md`（校准记录字节布局与系数语义） |
| `tools/` | PC 端工具，不参与固件编译：`calib_host.py`（RS485 主控，充当校准协议的外部发起方）、`parse_calib_dump.py`（把固件 `calib: dump` 行还原成 Rust 工具输入表） |
| `host/` | AD/DA 校准上位机（Tauri 2 + React + Vite + Tailwind），经 SWD/RTT **双向**控制 DUT：下行 `[0xA5,len,payload,crc32]` 命令帧（配置连线/下发参数/启动/暂停/继续/重启/停止/手动写码/读 AD/查状态/复检（`OP_HOST_VERIFY`，直接以 Flash 现记录跳转写盘后复检流程）/复位并运行/`OP_HOST_MANUAL_*`），上行解析固件 `calib:` 日志并可视化。侧边栏分页界面（概览/自动校准/复检/手动校准/参数/日志）：参数 tab 编辑 9 个门限/采样节拍/重测轮数 + 「跳过复检」开关（`localStorage["host.calibParams"]`），并在「配置/启动/重启/复检/手动写入」前自动下发 `OP_HOST_PARAMS`；概览卡展示 DUT 设备/版本/板型与 REF 全量信息（`calib: ref board=/no-reply` + 逐通道校准状态），手动 tab 驱动单板采集→拟合→落盘→复检，复检 tab 下发 `OP_HOST_VERIFY` 后展示逐通道 `verify max/@set/rmse` 与 `mode="verify"` 误差曲线（数据源 `verify_grid`/`verify_samples`），另有前误差/拟合残差/复检误差曲线。自动校准页按 DUT `board_dir` 切换标签：DA 方向 `DA chN ← REF AD chM`、AD 方向 `AD chN ← REF DA chM`。连接层：附着后强制目标运行（`core.status()/run()`）、RTT 找不到时按错误变体给排障 `hint`、`disconnect` 有界且不持 `conn` 锁、RTT 搜索有界（40 次后停手并结束工作线程释放会话，否则界面显示「连接」而后端回「已连接」，用户无法恢复）；探针掉线（拔 J-Link）时另发一次 `core.status()` 判定——probe-rs 扫描把内存读错误吞成 `ControlBlockNotFound`（`rtt::find_control_block` 内 `read(..).ok()?`），无法靠错误类型区分——随即结束线程并置 `error`；前端「连接/断开」按钮以会话存续（`connecting`/`searching`/`attached`）为准而非 `connected`，使搜索中的会话可被中止。**界面高度稳定性**（否则滚动容器内容变矮、浏览器把 `scrollTop` 钳到新上限，表现为「图表一刷新视图跳到最上面」，实测塌缩 442 px → 修复后 ≤10 px）：误差曲线卡片常驻且 SVG 外层容器固定 `aspectRatio`（有无数据同高，占位文案放在定高读数行）、图例/读数行 `h-5 overflow-hidden`、各表 `<table>` 带 `whitespace-nowrap`（`td` 上的 `white-space` 不继承到其 `display:block` 匿名子盒，必须加在表格元素上）、换 tab 时滚动容器 `scrollTo({top:0})`、日志自动滚动只写自身容器 `scrollTop`（`scrollIntoView` 会连带滚动祖先）。不参与固件编译，自带 `flake.nix` 与 `src-tauri`（probe-rs） |
| `prod/` | 产线烧录校准上位机（Tauri 2 + React + Vite + Tailwind，派生自 `host/`）。自动化单台流水：烧录 DUT 校准固件 → 附着 RTT → 自动校准（下行 PARAMS+CONFIG+START 帧、上行日志喂 `calib::Parser`）→ 复检 **PASS** 才烧录 APP 固件 → 附着 APP 校验启动日志（须见 `Device: … App Version: …`、`Init success`，以及本板标定记录加载标记 `calib ad|da load ok mask=…`（`mask` 须非 0）——板型取自校准阶段 `calib: board`，标记须与板型匹配，同时证明烧 APP 后校准扇区未坏）。流程结束（PASS/FAIL）后按钮变为「重新运行」，可对下一台板重复整条流水；流程线程退出即广播 `running=false`（前端据此隐藏「停止」）。**无独立「连接」步骤**：`开始流程` 内部自开探针会话（探针选择/芯片名/固件/参数全在 `PipelinePanel` 内），日志开关是右下角浮动按钮（`App.tsx`）；`prod` 的 `rtt.rs` 只留探针枚举 + 日志行解析 + `hint_for`，没有独立 RTT 监视线程与连接态。烧录用 `DownloadOptions{do_chip_erase:false, keep_unwritten_bytes:true}` 保护校准扇区 `0x0800F000`；REF 是产线夹具，本工具只处理 DUT。后端 `src-tauri/src/{flash.rs,pipeline.rs}`（`pipeline.rs` 纯函数 `verify_log_pass`（Device/App Version + Init success + 本板 `calib ad|da load ok mask≠0`）/`calib_load_ok`/`calib_pass` 有单测；`calib_pass` 在 `report.verify_skipped` 时以 `report.saved` 判定，即跳过复检后只要落盘即算 PASS），前端单页：`PipelinePanel`（配置 + 步骤条 + 参数区：9 个门限/节拍/重测轮数数字输入 + 「跳过复检」开关，`localStorage["prod.calibParams"]`）+ `CalibrationPanel`（DUT/REF 连接状态与状态卡 + 校准图表/结果），日志为可折叠抽屉（默认隐藏）。`src-tauri/src/calib.rs` 解析器与 host 同构（含坏点重测事件 `retries`：横幅 `retry`/`vretry` 只记轮次、逐点 `rtry`/`vrtry` 记事件，非致命），`CalibChart` 与 host `ErrorChart` 同构（重测标记 + 复检叠加开关 + 图例/读数行定高 + SVG 固定 `aspectRatio`），表格带 `whitespace-nowrap`，日志抽屉只滚自身容器——同 host 的界面高度稳定性处理（prod 无 tab 切换，故不需要切换回顶）。不参与固件编译，自带 `flake.nix` |
| `Board/<BOARD>/` | 板级代码：`Board.h/.cpp`、芯片驱动、`project/` |
| `Board/<BOARD>/project/{inc,src}/` | AT32 Workbench 生成：`main.c`、`at32f425_wk_config.c`、`at32f425_int.c`、`wk_system.c` |
| `Board/project/` | 新板型模板/存档，**不参与编译**（`BOARD` 从不等于 `project`） |
| `firmware/` | AT32 标准外设库实现（21 个 `.c`），原厂代码 |
| `include/` | `at32f425_conf.h`（外设裁剪）、`at32f425_int.h`（中断使能）、`at32f425_clock.h`，以及 `libraries/{drivers/inc,cmsis/cm4}` |
| `cmsis/` | `startup_at32f425.s`、`system_at32f425.c` |
| `cmake/gcc.cmake` | `arm-none-eabi-` 工具链定义 |
| `ldscripts/` | 链接脚本（含 `_Min_Heap_Size=0x3000`、`_Min_Stack_Size=0x400`） |
| `SVD/` | `AT32F425xx_v2.svd`，调试器寄存器视图 |
| `build/` | CMake 产物（已 gitignore） |
| `.github/workflows/build.yml` | GitHub Actions：固件 job（ubuntu，交叉编译全部 板型×角色×类型，打包成单个 `firmware-v<ver>.zip`）+ 桌面 job（matrix ubuntu/macos/windows × `host`/`prod`，`npm run tauri build` 出 deb/rpm/AppImage/dmg/msi/nsis）+ release job（`v*` tag 时先删同名旧 Release 再汇总上传）。不参与固件编译 |

## Development Commands

```bash
# 环境（Nix flake 提供 gcc-arm-embedded-15 / cmake / ninja / just）
direnv allow          # 或 nix develop

# 配置（BOARD = Board/ 下的目录名；BUILD_TYPE = Debug|Release；CALIB_ROLE = APP|REF|DUT|MANUAL，默认 APP）
cmake -DBOARD=DA_4CH -DBUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake -G Ninja -B build

# 校准固件（REF = 参考板, DUT = 待校准板）
cmake -DBOARD=DA_4CH -DCALIB_ROLE=REF -DBUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake -G Ninja -B build

# 手动校准固件（MANUAL = 单板：AD 板外接标准源采集 / DA 板写码后万用表读实测，按上位机「手动校准」tab 逐点）
cmake -DBOARD=AD_4CH -DCALIB_ROLE=MANUAL -DBUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake -G Ninja -B build/AD_4CH-MANUAL

# 构建
cmake --build build
# 产物: build/<BOARD>-Debug-v1.8.elf | build/<BOARD>-v1.8.elf (Release)
#       CALIB_ROLE 非 APP 时: build/<BOARD>-<ROLE>-Debug-v1.8.elf
#       build/<BOARD>-*.hex, build/<BOARD>.map

# 闭环精度检查，方案一（两块板 + PC 端脚本；REF 之间不互相发起通信，须由脚本驱动；需 pyserial + USB-RS485 适配器）
# 若适配器跑不到 6 Mbps，两板 USART2 改 1000000 后加 -b 1000000
python3 tools/calib_host.py -p /dev/ttyUSB0 ping
python3 tools/calib_host.py -p /dev/ttyUSB0 read-ad          # 4 路 ideal
python3 tools/calib_host.py -p /dev/ttyUSB0 sweep --gate 16  # 67 点闭环，门限 16 code

# DUT 写盘后自动复检 + dump 导出（AD_4CH=DUT, DA_4CH=REF 上电即可）
# 默认 DUT 上电停在 S_IDLE 等上位机 start；加 -DCALIB_AUTO_START=1 恢复「上电即自动」全流程
cmake -DBOARD=AD_4CH -DCALIB_ROLE=DUT -DBUILD_TYPE=Debug -DCALIB_AUTO_START=1 -DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake -G Ninja -B build/AD_4CH-DUT
cmake --build build/AD_4CH-DUT
just run build/AD_4CH-DUT/AD_4CH-DUT-Debug-v1.8.elf 2>&1 | tee /tmp/dut.log
# 日志含: N×「run ch= da=」-> 单次 67 点并行扫描（每点每通道一行 `calib: dpt<idx> ch=<c> ideal= meas=`）-> `FitAll` 逐通道 `chN resid` -> dump(grid/chN/coeff) -> saved + `mask 0x%X` -> 单轮 67 点并行复检（`calib: dvfy<idx> ch=<c> set= meas=`）-> 汇总 PASS/FAIL
python3 tools/parse_calib_dump.py /tmp/dut.log --ch 0 > /tmp/ad0.txt
(cd ../adc_poly_calibration && ./target/debug/adc-poly calib --input /tmp/ad0.txt \
    --mode code --x-col 0 --y-col 1 --code-min 0 --code-max 65535 --phys-min 0 --phys-max 10 --degree 3)
# 判据: Rust 的 `补偿后 max/rmse` 应等于固件的 `ch0 resid max/rmse`（逐位一致即证明拟合无误）
# 上电即复检: cmake ... -DCMAKE_CXX_FLAGS=-DCONFIG_VERIFY_ON_BOOT -B build/AD_4CH-DUT-boot

# 采集链路定位（编译开关，默认全关；详见 Testing & QA）
# -DCALIB_LOG_SPAN=1        DUT 每点追加 `calib: span%u min=… max=…`（窗内极差）
# -DCALIB_FIXED_IDEAL=25000 67 点全钉在该 ideal 上驻留，分离「施加不稳」与「采集不稳」
# -DCALIB_AUTO_START=1      非空 = DUT 上电自动按默认配置（4 通道，REF ch0）启动；默认等待上位机 start
# REF 侧 `calib: set_da ch= ideal= raw=` 恒开，用于核对实际施加码
cmake -DBOARD=AD_4CH -DCALIB_ROLE=DUT -DBUILD_TYPE=Debug -DCALIB_LOG_SPAN=1 -DCALIB_FIXED_IDEAL=25000 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake -G Ninja -B build/AD_4CH-DUT-dwell
cmake --build build/AD_4CH-DUT-dwell

# 烧录 / 调试（probe-rs，chip=at32f425k8t7）
just download build/DA_4CH-Debug-v1.8.elf   # 仅烧录
just run      build/DA_4CH-Debug-v1.8.elf   # 烧录并运行，日志经 RTT 打在终端（无需串口）
just attach   build/DA_4CH-Debug-v1.8.elf   # 不下载，仅连上看 RTT 日志
just erase | reset | info
just all                                     # 全部板型 × 角色 × 类型 并发配置 + 构建
just configure_all | build                   # 仅并发配置 / 仅并发构建
just make AD_4CH DUT Debug                   # 配置并构建单个组合
just watch AD_4CH DUT Debug                  # watchexec 增量重建
```

上位机（`host/`，独立于固件工具链，自带 `flake.nix`）：

```bash
git add host/                 # 一次性：嵌套 flake 仅对 git 已跟踪文件可见
just host-install             # 进 host 的 nix shell 安装前端依赖
just host                     # 启动桌面应用；点「连接」后 RTT 日志/校准结果实时刷新
# 连接 DUT 后：勾选 AD 通道 + 选 REF DA 源 →「配置」→「启动」；另有暂停/继续/重启/停止、手动写码/读 AD/刷新状态
# 「参数」tab：9 个门限/采样节拍/重测轮数 + 「跳过复检」开关 →「应用参数」（之后配置/启动/复检/手动写入会自动先下发）；固件回执显示在「设备已应用」
# 连接恢复：状态『error』时点『复位并运行』；连接 MANUAL 固件后切「手动校准」tab：开始 → 采点 → 拟合 → 写入 Flash → 复检
# 下行命令 [0xA5,len,payload,crc32(LE)]，opcode 见 calib/CalibOps.h 与 host/src-tauri/src/proto.rs
just host-build               # 前端 tsc + vite 构建
just host-test                # 日志解析器单测（无需硬件）

# 等价手动方式
cd host && nix develop --command npm run tauri dev
```

- `host/` 上位机用 probe-rs 独占探针，运行期间**不要**同时执行 `just run` / `just attach`；无硬件时用界面「加载日志」回放 `just run … | tee dut.log` 的日志。

产线烧录校准上位机（`prod/`，独立于固件工具链，派生自 `host/`，自带 `flake.nix`）：

```bash
git add prod/                 # 一次性：嵌套 flake 仅对 git 已跟踪文件可见
cd prod && nix develop --command npm install
cd prod && nix develop --command npm run tauri dev    # 启动桌面应用
cd prod && nix develop --command npm run build        # 前端 tsc + vite 构建
cd prod && nix develop --command cargo test --manifest-path src-tauri/Cargo.toml  # 纯函数单测（无需硬件）
```

- 生产流程：选探针/芯片 → 选 DUT 校准固件 + APP 固件 ELF/HEX + ch_mask + REF DA 源（+ 门限/节拍参数区）→ 「开始流程」。流程见 `prod/README.md`。
- 核心不变式：**复检 PASS 才烧录 APP**；FAIL/超时/连接失败即停止。烧录保护校准扇区 `0x0800F000`。勾选「跳过复检」时该不变式退化为「落盘即放行」（判据是拟合残差门限，未落盘/`phase=fail` 仍直接中止）。
- 与 `host/` 共用 probe-rs 独占探针约束，运行期间不要同时执行固件仓库 `just run`/`just attach`。

- 版本号在 `CMakeLists.txt` 的 `MAJOR` / `MINOR`，会进入产物文件名与 `CONFIG_*_VERSION` 宏；`FWConfig.h` 有独立兜底默认值，改版本需两处一致。
- `.vscode/tasks.json` 列出了 8 个板型的 Configure 任务，但多数板型目录已不存在——**以 `Board/` 实际目录为准**。
- 注意 `.vscode/tasks.json` 被误存为 `develop.md`（同名 JSON 内容），修改任务时勿改错文件。

## Code Conventions & Common Patterns

- 格式化 `.clang-format`：BasedOnStyle Google，`IndentWidth 4`、`ColumnLimit 120`、`AccessModifierOffset -4`。
- 静态检查 `.clang-tidy`：`bugprone-*`、`cppcoreguidelines-*`、`modernize-*`、`performance-*`、`readability-*`；命名规则要求**变量/函数/参数 `lower_case`**（与现存 `PascalCase` 方法名存在冲突，改代码时不必强行对齐命名检查）。
- 头文件一律 `#pragma once`。
- 命名：类 PascalCase（`Application`、`SoftTimer`），成员 `m_` 前缀 + camelCase（`m_oneMsTimer`），全局/桥接函数 PascalCase（`ApplicationInit`、`SoftTimerTick`）。
- 注释默认不写；仅在 WHY 不明显时加一行，中英混排可接受。
- C/C++ 边界：对外 C 接口用 `extern "C"`。`main.c` 是 C，只能通过 `CPPInterface.h`（`ApplicationInit` / `SwTmrTick`）进入 C++。
- 单例：Meyer's `static X& GetInstance()`（`Application`、`Logger`、`SoftTimer`），并常配一层 C 函数包装（`SoftTimerTick`、`AppOneMsTask`、`SoftTimerLoop`）。
- 外设初始化统一经 `DrvInit` 静态方法（`PinInit` / `Pinmux` / `UartInit` / `DmaInit` / `ClockEnable` / `IrqEnable` / `ExintInit`），不要直接裸调 `at32f425_*.h`，除非在原厂库里。
- 抽象基类显式虚析构（`BaseBoard`、`Timer`、`Delay`、`OutStreamAbstract`），子类 `override`。
- 内存：驱动持有者用 `std::unique_ptr`（`AD_4CH` 的 `m_ad7689Drv`）或按值成员（`DA_4CH` 的 `m_DACS0..3`）；`SerialOutStream` 缓冲区为裸 `new[]`，在构造/析构中成对释放。
- 错误处理：无异常、无错误码，靠 `bool` 返回 + 日志；不写无实际价值的防御代码。
- 定时任务：TMR1 硬件 1ms 时基 → `SoftTimer` 链表（`ONESHOT` / `PERIOD`）。
- Workbench 生成文件：用户代码只能写进 `/* add user code begin X */ ... /* add user code end X */` 区段，否则重新生成会被覆盖。
- 遗留文件 `user/main.cpp.bak`、`user/InterruptCallback.cpp.bak` 不参与编译（glob 只匹配 `.c`/`.cpp`），属旧版参考。
- `AD7689Drv::m_filterData` 是盒式滤波器环形槽但从未初始化，`GetAveragerVal` 对全部 `FILTER_NUM` 槽求平均：上电后第一圈未填满时均值混入 RAM 残留，首个采样点可偏离真值 ~170 码（实测 `pt0 raw=32` 而真值 201），被三次拟合吸收后整条低段系统性偏移、复检期才暴露。`Init()` 末尾跑 `FILTER_NUM*4` 次 `Sync()` 预热，槽填满后计数器自然回绕到 0。

## Important Files

| 文件 | 作用 |
| --- | --- |
| `CMakeLists.txt` | 板型矩阵、目标划分（`at32` / `at32_itf` / `app` / exe）、版本号、编译与链接选项 |
| `cmake/gcc.cmake` | 工具链与自定义 compile/link 命令行 |
| `Board/<BOARD>/project/src/main.c` | 固件入口（Workbench 生成） |
| `user/Application.{h,cpp}` | 应用生命周期与主循环，`Board` 的具体子类 |
| `user/BaseBoard.h` | 板型抽象接口 |
| `user/CPPInterface.{h,cpp}` | C↔C++ 桥接（`ApplicationInit`、`SwTmrTick`） |
| `user/FWConfig.h` | 板型宏分发（新增板型必改） |
| `Board/<BOARD>/Board.h` | 板型声明，被 `Application.h` 按 include 路径解析 |
| `calib/CalibProc.{h,cpp}` | 校准状态机（`S_IDLE`/PING→扫描→拟合→dump→落盘→复检→DONE|FAIL）、LED 指示、上位机下行命令解析（`HostPoll`/`HandleHost`/`HandleManual`）；REF/DUT/MANUAL 行为在此分支；DUT 支持按通道位掩码并行校准、暂停/停止；MANUAL 为单板手动校准（采集/拟合/落盘/回读复检）。DUT/MANUAL 的 `OP_HOST_STATUS` 应答除 role/state/mask/coeff 外重复输出 `Device: …, App Version: …`、`calib: board …`、`calib: loaded`——上电日志只在复位后出现一次，重连（不复位 MCU）靠该应答补齐上位机概览卡字段。门限与采样节拍为**运行时参数**（`OP_HOST_PARAMS` 下发，不落 Flash、复位回默认，见 `ApplyParams`）：复检门限/拟合残差门限/不稳定极差/建压等待/首点建压/AD 与 DA 采样间隔/采样次数（1..`kWindowMax`=32）/重测轮数（1..`kRetryCountMax`=9），`kDef*` 常量仅作默认初值（`CALIB_SETTLE_MS`/`CALIB_FIRST_SETTLE_MS` 编译开关覆盖默认）；`skip_verify` 置位时 `DoSave` 落盘后直接 `calib: verify skipped` → `S_DONE`（以拟合残差门限放行，不复检） |
| `calib/CalibOps.h` | 共享 opcode 与帧常量：REF 应答协议（`OP_PING`/`OP_SET_DA`/`OP_READ_AD`/`OP_GET_INFO`/…）与上位机下行协议（`OP_HOST_CONFIG`…`OP_HOST_STATUS`/`OP_HOST_VERIFY`/`OP_HOST_PARAMS`、`OP_HOST_MANUAL_*`） |
| `calib/CalibLink.{h,cpp}` | USART2 + DMA1_CH1 环形收、阻塞发；帧格式 `0xA5,len,payload,crc32`（常量取自 `CalibOps.h`） |
| `calib/CalibStore.{h,cpp}` | `0x0800F000` 校准记录 v2（含 `validMask`）读写与校验，`CalibCrc()` 共享 CRC-32 实例 |
| `calib/PolyFit.{h,cpp}` | 三次多项式最小二乘拟合 / Horner 求值 |
| `docs/calibration-flash.md` | 校准记录字节布局、`board` 语义、CRC 与解码示例 |
| `tools/calib_host.py` | PC 端 RS485 主控脚本：`ping` / `set-da` / `read-ad` / `sweep`（闭环精度门限） |
| `tools/parse_calib_dump.py` | 把固件 `calib: dump {grid,chN}` 行还原成 Rust 工具输入表（`--ch N`），供拟合复算比对 |
| `user/DrvInit.{h,cpp}` | 外设初始化封装 |
| `user/SoftTimer/SoftTimer.{h,cpp}` | 软定时器调度 |
| `user/SerialOutStream.{h,cpp}` | USART4 日志环形缓冲，同时把每条日志拷进 RTT 上行通道 |
| `user/Rtt.{h,cpp}` | SEGGER RTT 控制块 `_SEGGER_RTT`（1 条 1 KB 上行 `log` + 1 条 256 B 下行 `cmd`）；日志除 USART4 外走 SWD 上行，下行经 `RttRead()` 读给 `HostPoll`（DUT） |
| `host/src-tauri/src/proto.rs` | 上位机下行帧编码，对齐固件 `[0xA5,len,payload,crc32(LE)]`；opcode 与 `CalibOps.h` 一致；`CalibParams`/`params()` 编码 18 B 参数帧（`OP_HOST_PARAMS`） |
| `host/src-tauri/src/rtt.rs` | probe-rs attach + RTT 双向循环：附着后强制目标运行，每轮下发 `commands` 队列命令（`down_channel`）再读上行（`up_channel`）切行；含控制通道（复位并运行/仅运行）、RTT 找不到的错误分类 `hint` 与有界搜索 |
| `host/src/components/OverviewTab.tsx` | 概览卡：连接状态 + DUT 设备/版本/板型/系数 + REF 全量信息与逐通道校准状态 |
| `host/src/components/ManualPanel.tsx` | 手动校准 tab：AD 采集点 / DA 写码录值 → 拟合 → 写入 Flash → 复检展示 |
| `host/src/components/ControlPanel.tsx` | 连线选择（按 DUT 板型方向：`AD chN←REF DA chM` / `DA chN←REF AD chM`）+ 流程按钮 + 手动写码/读 AD（`raw` 或 `校准后` 二选一，对应 `OP_MODE_RAW/CAL`）+ 每通道有效性状态 |
| `host/src/components/VerifyTab.tsx` | 复检 tab：连线选择（复用 `ChannelLinkMatrix`）→ `calib_verify`（下发参数 → CONFIG → `OP_HOST_VERIFY`）→ 展示逐通道 `verify max/@set/rmse` 与 `ErrorChart mode="verify"`；非 DUT 角色 / 无 Flash 记录给警示 |
| `host/src/components/ErrorChart.tsx` | 手写 SVG 误差曲线（无图表库）：`mode="sweep"` 画前误差（灰）+ 后误差（蓝）+ 复检叠加（绿虚线，`叠加复检` 开关，`vAfter` 用落盘系数对 `verify_samples` 求值）；`mode="verify"` 只画复检误差。坏点重测标记：琥珀圆点 + 「旧样本偏差→重测偏差」竖线（`dropped` 时空心 = 保留旧值），悬停读数追加 `重测N轮 采纳/保留旧值(新=… 旧=…)`，图例显示 `重测 N 轮 / M 点（保留旧值 K）`。高度恒定：SVG 外层固定 `aspectRatio: 860/340`，图例/读数行 `h-5 overflow-hidden` |
| `host/src/components/CalibResult.tsx` | 自动校准 tab 结果表（前误差 / 拟合残差 / 复检 max·rmse / 系数 c0..c3）；`<table>` 带 `whitespace-nowrap`（见 `host/` 行的高度稳定性说明），数据到达时行高不变 |
| `host/src/components/LogConsole.tsx` | 日志 tab：过滤 / 自动滚动 / 加载日志文件；自动滚动只设自身容器 `scrollTop = scrollHeight`，不用 `scrollIntoView`（那会连带滚动祖先滚动区） |
| `host/src/components/ParamsTab.tsx` | 参数 tab：9 个门限/节拍/重测轮数数字输入 + 「跳过复检」开关 + 「应用参数/恢复默认」；展示固件回执 `report.fw_params`（设备已应用） |
| `host/src/hooks/useCalibParams.ts` | 参数状态（`localStorage["host.calibParams"]`，`DEFAULT_PARAMS` 与固件 `kDef*` 一致）；`apply()` → `calib_params` 命令（`OP_HOST_PARAMS`） |
| `user/BaseTimer.{h,cpp}` / `user/Timer/Timer.h` | 硬件定时器抽象 |
| `include/at32f425_conf.h` | 外设/中断裁剪开关 |
| `ldscripts/AT32F425x8_FLASH.ld` | 内存布局与堆栈下限 |
| `justfile` | 编译/构建（`all`/`configure_all`/`build`/`make <BOARD> <ROLE> [TYPE]`，目录 `build/<BOARD>_<ROLE>_<TYPE>`，并发）+ 烧录/复位/监视 + `host-*`/`prod-*` 任务 |

## Runtime/Tooling Preferences

- 交叉编译：`arm-none-eabi-gcc`（Nix `gcc-arm-embedded-15`），`-mcpu=cortex-m4 -mthumb -O2`；链接 `--specs=nano.specs -u _printf_float --specs=nosys.specs`、`-s`、`--gc-sections`。
- `-s` 对所有构建生效，**ELF 一律无符号表**：probe-rs/J-Link 找 `_SEGGER_RTT` 只能扫内存（`--rtt-scan-memory`；J-Link RTT Viewer 勾选 Search / 填地址）。RTT 上行环形缓冲 1024 B、SKIP 模式无流控：突发写入快于主机读取时静默丢字节（`RttWrite` 在 `next == rd` 处 `break`）。任何往 RTT 打大段内容的代码必须自行分次 + 留间隔（见 `CalibProc.cpp` 的 `DoDump()`，`kLineGapMs`），否则行会被截断或整行被覆盖。
- 构建系统：CMake ≥ 3.13 + Ninja（`CMakeCache.txt` 中 `BOARD` / `BUILD_TYPE` 为 string cache 变量）。
- `build/` 是**共享**目录：切换 `-DBOARD` 会复用同一目录并残留上一次的产物，干净构建请删除或换目录。
- `CMakeLists.txt` 的 `APP_INCLUDE` 含硬编码 Windows 路径 `E:/develop/AT32IDE/.../arm-none-eabi/include`（AT32IDE 自带 GCC 内建头）。在 Linux/macOS 下这些路径不存在但仅作 `-I` 传入，不影响构建；新增编译器内建头依赖时不要依赖这些路径。
- 烧录/调试：probe-rs（`justfile`，chip `at32f425k8t7`，SWD）；`.vscode/launch.json` 另有 probe-rs-debug 与 J-Link cortex-debug 两套配置（其 `programBinary` 仍指向不存在的 `DIG_8IN_8OUT_NPN-Debug-v1.8.elf`，使用前需改）。
- `.gitignore` 忽略 `.vscode/`、`*.code-workspace`、`build/`、`.direnv/`、`.codegraph/`、`out/`，因此编辑器与工作区配置是本地状态，不要假定他人可见。
- 仓库同时保留 Eclipse/AT32IDE 元数据（`.project`、`.cproject`、`.settings/`、`preferences.ini`、`*.ATWP`）——仅供原厂 IDE 打开，不是构建链的一部分。

## Testing & QA

- **无测试框架、无宿主可运行目标**。不存在 `test/` 目录、Unity/CppUTest/gtest。CI（`.github/workflows/build.yml`）三个 job：`firmware`（ubuntu 单平台，编译全部 `BOARD∈{AD_4CH,DA_4CH}` × `CALIB_ROLE∈{APP,REF,DUT,MANUAL}` × `Debug/Release`，打包成单个 `firmware-v<MAJOR>.<MINOR>.zip`——固件是 `arm-none-eabi-` 交叉编译产物，与构建主机平台无关，故不分平台；工具链 `carlosperate/arm-none-eabi-gcc-action` 固定 `14.2.Rel1`）；`desktop`（matrix `ubuntu/macos/windows` × `host`/`prod`，`npm ci` + `npm run tauri build`，出各平台原生安装包）；`release`（仅 `v*` tag，先用 `gh release delete` 清掉同名旧 Release 再汇总上传，保证重打 tag 不留陈旧附件）。无 `.gitlab-ci.yml`、`Jenkinsfile`。桌面 job 未在本地验证过（需 webkit2gtk 等系统依赖与完整 Rust/node 构建），以 GitHub runner 实际结果为准。
- 唯一的质量门是构建成功 + 静态检查配置（`.clang-tidy` 已配置但未接入自动化流水线；`cppcheck` 无配置）。
- 验证手段只能是硬件在环：`cmake --build build` → `just run <elf>` → 通过 USART4 日志（`LogInfo`/`LogTrace`）与 SWD 调试观察行为。改动驱动/标定/定时逻辑后，必须说明需要哪块板卡验证。
- AD 标定实测（AD_4CH=DUT, DA_4CH=REF）：**残差与 AD/DA 通道对应关系直接相关**，实测证据如下。
  - **实测对照**：4 个 AD 通道共接 1 个 DA 输出时 `ch1 resid max=28875 @58000 rmse=15436`、低段残差呈 `12500→28125` 线性递减（ch0 反而 `resid max=1.5 @37000 rmse=0.5`）；改为只接 1 路 AD 后 `resid max=1.5 / rmse=0.5`。非 ch0 通道的 ideal→raw 映射本来就只对 ch0 成立（无校准记录时 `CalibDaRaw` 对非 ch0 原值直通），共接时这些通道采到的不是自己被驱动的电压。
  - **此前诡异现象的成因（已定位并修复，见下「AD 方向采样估计量」）**：四通道同号同步的 ~160 码跳变、跳变位置逐次运行随机且扫描期与复检期不重合、`s_settleMs` 加大反而更差、`FILTER_NUM` 改小无改善——均与「AD 方向窗内估计量对单个异常样本敏感 + 窗长短于滤波记忆长度」一致，与施加端/接线无关。
  - **AD 方向采样估计量（v1.10 修复）**：AD 方向扫描/复检（`DoSweep`/`DoVerifySweep` 的 `CONFIG_DEVICE_AD_4CH` 分支）与 MANUAL 采点（`ManualTick`）此前用「窗内 min/max 中值」`0.5*(min+max)`，窗内任一异常样本按**一半幅度**直接进读数（离线复核：16 点窗注入 3 个 −6000 毛刺 → `mid` 偏 −3000、中值偏 0），这就是实测 `resid max=5681.5 @61000`（单点读数偏 ~2900 码，恰为窗内 ~5800 码极差的半幅）与 ~150 码级瞬跳的来源。且默认 `s_sampleIntervalMs=5` 使窗长 16×5=80 ms **短于** AD7689 均值滤波的 128 ms 记忆，坏样本可污染整窗（`FILTER_NUM` 改小反而把单个毛刺在均值里的权重从 1/32 放大到 1/4，故实测无改善）。现三条路径统一改用 `MedianWin()`（与 DA 方向同一估计量），`kDefSampleIntervalMs` 默认 5→20（窗长 320 ms > 128 ms），`s_stableMin/Max` 只保留给 `unstable` 标记。默认值同 `ref_sample_interval_ms` 有四处副本（固件 `CalibProc.cpp`、`proto.rs` `impl Default`、host `useCalibParams.ts`、prod `PipelinePanel.tsx`）——已存 `localStorage` 的上位机不会自动更新，需在「参数」tab 点「恢复默认」或手填 `samp=20`，否则仍按 5 下发、窗长退回 80 ms。
  - **正确接法**：标定时 DUT 每个 AD 通道必须各自接到 REF 的一个独立 DA 通道；只想验证单路时只接 ch0 对应的那一路。共接时非 ch0 通道的残差会大得离谱（实测 28875 码 ≈ 44% FS），`FitAll` 在各通道重测轮数用尽后 `EnterFail()`，不落盘任何系数——这是设计行为（接线错的板子只是多跑几轮重测），看到这种量级先查接线，不要调门限。
  - **顺带修复的独立缺陷**：`AD7689Drv::m_filterData` 从未初始化，`GetAveragerVal` 对全部 32 槽求平均，上电后第一圈（128 ms）未填满时均值混入 RAM 残留。同一次运行 `pt0 ideal=0 raw=32`，而复检同一 `set=0` 的 `vfy0 raw=201`（两次 DA 都是 0）；把 pt0 从 32 改成 201 后离线重拟合，复检低段 d 从 `+41.4/+34.5/+29.3/+17.8`（ideal 0/1000/2000/5000）降到 `+3.3/+1.0/−0.3/−1.6`。修复：`Init()` 末尾跑 `FILTER_NUM*4` 次 `Sync()` 预热，槽填满后 `cnt1` 自然回绕到 0。
  - **已用硬件数据排除的假设**：建压时间（`s_settleMs` 200→600 反而更差）；AD 盒式窗深度（`FILTER_NUM` 32→4 无改善）；通道轮转/`t_ACQ`（单通道连发绕过 4 ms 轮转无改善）；REF ACK 时序（ACK 在 `CalibDaSet()` 之后才发）；拟合方向与系数（离线逐位复算一致）；SPI 位/CFG 布局（对照 AD7689 手册 Table 13 逐位核对正确）；扫描/复检两段 `PH_SETTLE` 逐行对比一致；`s_stableCount`/`s_stableMin/Max` 初值一致；`s_rec.coeff` 与 `s_coeff` 同源（`DoSave` 只读不改）；`CalibLink` 环形缓冲与 CRC；内建校准曲线（已移除，问题段斜率平滑单调）。
  - **坏点重测（v1.12）**：随机尖峰（~150 码级瞬跳，成因见上节）会让单点偏差超门限。扫描与复检统一采用「只重测超标点」策略：扫描在 `FitAll` 后收集 `|resid| > s_residualLimit` 的点（`CollectOffenders()`，逐点记下超标通道位），复检在汇总后收集 `|d| >= s_checkGate` 的点（`CollectVerifyOffenders()`），只重测这些点，含首测共 `s_retryCount` 轮（默认 3），用尽仍超标才 `EnterFail()` / `verify FAIL`。日志：`calib: retry <轮>/<上限> n=<点数>` 与 `calib: vretry <轮>/<上限> n=<点数>`（每轮开头），逐点 `calib: rtry<idx> ch= ideal= meas= d= old=` / `calib: vrtry<idx> ch= set= meas= d= old=`（` drop` 后缀 = 保留旧样本）。两个不变式：(1) **重测只覆盖超标通道**（`s_rsMask`），已合格通道的样本不动；(2) **重测值只在与当前拟合（复检用落盘系数）更贴合时才采纳**，否则保留旧样本——故一轮重测不可能新增坏点，最坏白跑一轮。重测点建压用 `s_settleMs`（idx 0 仍用 `s_firstSettleMs`）。**失败诊断行（`chN resid>` / `vfy chN |d|>=`）只在用尽轮数后打印**：上位机与产线把这两行判为 `phase=fail` 并立刻中止，中途的重测行不得触发（host/prod 解析器各有 `retry_lines_are_non_fatal` 单测守住该不变式）。MANUAL 逐点采集由上位机驱动，不在本机制内（尖峰需人工重采）。耗时上界：每轮重测 ≤ 67 点 × ~0.52 s ≈ 35 s（AD/DA 方向同量级），默认 3 轮 → 扫描与复检各 ≤ ~105 s；`retry_count` 调到 9 时最坏 ~630 s，会超过 prod 的 `CALIB_TIMEOUT_SECS=600`——产线保持默认 3。硬件验证：正常流程只在真出现尖峰时打 `retry`/`vretry`（应能看到 `rtry`/`vrtry` 与随后的 `chN resid max` 变小）；要**确定性**触发重测可用 `-DCALIB_SETTLE_MS=1` 故意欠建压——多数点会超标，3 轮后仍 `chN resid>` + `state fail`，可一并验证「重测用尽才失败」与上位机不误报。
  - **上位机可视化与界面高度稳定性（v1.12 配套）**：host/prod 解析重测行成 `RetryEvent`（`verify/round/idx/ch/at/meas/d/old/dropped`，见 `CalibReport.retries`）并画在误差曲线上——扫描图画前误差（灰）+ 后误差（蓝）+ 复检叠加（绿虚线，可开关），重测点画「旧样本偏差→重测偏差」竖线与琥珀圆点（`dropped` 空心 = 保留旧值），图例行给 `重测 N 轮 / M 点（保留旧值 K）`，悬停读数追加 `重测N轮 采纳/保留旧值(新=… 旧=…)`。这些行**非致命**：解析器更新轮次/记录事件但不得置 `phase=fail`、不进 `errors`（host/prod 各有 `retry_lines_are_non_fatal` / `retry_round_keeps_run_passing_with_fresh_resid` 单测守住；只有用尽轮数后的 `chN resid>` / `vfy chN |d|>=` 才判失败）。另一处独立修复：**图表刷新时视图跳到最上面**——根因是卡片在 `calib: reset`（新一轮清空）时高度骤减（无数据占位 → 340 px SVG、表格系数/误差单元格折行），滚动容器内容变短后浏览器把 `scrollTop` 钳到新上限（DOM 实测内容 1336→894 px、滚动 501→59）。修复＝消除可控高度收缩（图表卡片常驻 + SVG 容器固定 `aspectRatio` + 图例/读数行 `h-5 overflow-hidden` + 表格 `whitespace-nowrap` 加在 `<table>` 上，因 `td` 的 `white-space` 不继承到其 `display:block` 匿名子盒）+ 换 tab `scrollTo({top:0})` + 日志自动滚动只写自身容器；修复后实测同一场景塌缩 442 px → ≤10 px。UI 验证用真实应用（`just host` / prod 的 `npm run tauri dev`），两应用 vite 端口同为 1420 不能同时开。
  - **残差门限**：拟合残差门限 `s_residualLimit` 默认 20（`kDefResidualLimit`），重测轮数 `s_retryCount` 默认 3（`kDefRetryCount`，运行时 1..`kRetryCountMax`=9）——四条默认值副本：固件 `CalibProc.cpp`、`proto.rs` `impl Default`、host `useCalibParams.ts`、prod `PipelinePanel.tsx`；复检门限 `s_checkGate` 默认 16；两者自 v1.9 起可由上位机经 `OP_HOST_PARAMS` 运行时覆盖（不落 Flash）。取值依据：单路正确接法实测 AD `resid max=1.5 / rmse=0.5`（降到 20 仍有 13× 余量）、DA 方向扫描/复检 ~14 码（20 只剩 ~1.4× 余量，靠重测兜底；DA 方向若实测反复触发 `resid>`，把该值临时调回 50~100）；接线错误时残差达 28875（44% FS）仍被挡住。`FitAll` 超限时打 `calib: ch%d resid>%d at: <ideal>(<dev>)…`（LogError，列出全部超标点）后 `EnterFail()`，`EnterFail()` 自身打印 `calib: state fail`——所有失败路径（ref no-reply/mismatch、read_ad timeout、落盘失败、残差超限、复检 `chk fail`）唯一的端子行，上位机 `calib.rs` 把 `state fail` 与 `resid>` 行都判为 `phase=fail`，prod 据此立刻中止流程并按 FAIL 处理（不再空等 `CALIB_TIMEOUT_SECS=600` 超时）。注意门限降到 20 后 ~150 码级异常必被 `resid>` 挡下（`EnterFail`，不落盘）——但前提是重测轮数已用尽：该量级瞬跳在 v1.10 前源于 AD 方向窗内估计量缺陷（见上「AD 方向采样估计量」），重测后仍出现就先查共地/接线与施加端，别急着把门限调回 1000（临时放宽可在上位机「参数」tab 下发，不落 Flash；prod 的门限持久在 `localStorage["prod.calibParams"]`，改代码默认值不动已存条目——已存的 `residual_limit` 仍是 100，要吃 20/3 要在「参数」tab 点「恢复默认」）。若实测残差到几十~上百码，先查 AD/DA 通道是否一一对应，不要先调门限。
  - 定位工具已内置（编译开关，默认关闭，均在 `CMakeLists.txt` 定义为 cache 变量并经 `target_compile_definitions` 传入）：`-DCALIB_LOG_SPAN=1` 让 DUT 在扫描段每点（各通道 `calib: dpt… ch=` 行之后）追加 `calib: span%u min=… max=…`（四通道各自的窗内 min/max，用于判断「电平在窗内是否稳定」；复检段的 `vspan` 已随并行化移除）；`-DCALIB_FIXED_IDEAL=<code>` 把 67 点全钉在该 ideal 上驻留（分离「施加不稳」与「采集不稳」）；`-DCALIB_SETTLE_MS=<ms>` 覆盖默认 `kDefSettleMs`；`-DCALIB_FIRST_SETTLE_MS=<ms>` 覆盖默认 `kDefFirstSettleMs`（首点建压等待，默认 600 ms——扫描/复检起点 DA 大跳变，`s_settleMs` 不足会使低端 0 点锚点测偏 ~30 码；标定与复检两侧共用此常量）；`-DAD_FILTER_NUM=<n>` 覆盖 `AD7689Drv::FILTER_NUM`（盒式窗深度）；`-DAD_BURST_SAMPLE=1` 让 `Sync()` 改走 `ReadOneChannelData()`（每通道连发 3 帧、间隔 1 ms，绕过 4 ms 轮转）。REF 侧 `OP_SET_DA` 恒打印 `calib: set_da ch= mode= in= raw=`（REF 实际写给 MS5542 的码；`mode=0` 表示 `in` 即 raw 直写，`mode=1` 表示已按 ideal 查系数）。
- DA 方向（DA 板 DUT + AD 板 REF）实测：REF 经 `OP_READ_AD` 回传的 ideal 是 **AD 板标定后**的理想码，其域可略超 65535（实测满量程 `calib: refad ch=0 ideal=65930.2`，即 AD 标定曲线在全码段斜率 ≈1.0057）。故 DUT 侧 `calib: dpt… meas=` / `calib: dvfy… meas=` 以 `%.1f` 打印（有符号浮点，可达 ~66000、也可为负——早先用 `(unsigned)` 截断会把负值打成 0，掩盖首点差异），`dump chN` 仍按无符号整数打印（可达 ~66000），上位机 `samples` 以 f64 解析，勿强制 u16（u16 溢出会静默变 0，令「前误差」假报 65535）。DA 方向复检 `verify ch%d max/rmse` 与扫描残差同量级（实测 ~14 码），远小于门限。DA 方向每点采样 = 等 `s_settleMs`(默认 200 ms) 建压后，**按 `s_refSampleIntervalMs`(默认 20 ms) 间隔**连读 16 次 REF `OP_READ_AD`（窗长 320 ms），窗内取**中值**（非 mid(min,max)）。两点不可省：(1) REF 的 AD 值是 32 槽均值滤波输出、每 4 ms 刷新一次，采样必须跨过滤波长度；(2) 单个坏 ADC 样本进入该滤波后会把输出整体抬升 ≈5000/32≈156 码并持续 128 ms，只要窗长 > 128 ms，被污染样本即为少数，中值可完全剔除——实测复检随机 ~150~260 码超差（`vfy chN |d|>=16 at: 62000(148)` 之类，每轮随机通道/点位）即源于此；窗内极差 > `s_stableCode`(默认 20) 时该点 `dpt`/`dvfy` 行追加 `unstable`。
- DUT 写盘后复检（`S_VERIFY`）：用刚写入 Flash 的记录**单轮 67 点并行**重跑全部选中通道（每点每通道一行 `calib: dvfy<idx> ch=<c> set= meas=`，随后 `vfy set=… ch… d=…` 逐通道判定），末尾 `verify ch%d max=… @set=… rmse=…`（**一行 4 通道，缓冲区须 ≥256 B**；早先 128 B 会静默截断末通道，令上位机该通道复检列显示 `—`）+ 超 `s_checkGate`(默认 16) 点清单 + `PASS`/`FAIL`。超门限点先按上述重测机制（`vretry`/`vrtry`，含首测共 `s_retryCount` 轮）重测，用尽才出 `|d|>=` 清单与 `FAIL`；复检聚合量（逐通道 `max/@set/rmse`）由 `VerifyRecalc()` 从 `s_verify` 重算（重测会替换样本，不再增量累积），重测点不打 `vfy set=` 逐点行。仅出日志结论，**不回滚系数**（FAIL 不擦 Flash、不改 LED）。复检 `max` 略高于扫描 `resid`（实测 164 vs 133）属训练误差→泛化误差的正常增量。`CONFIG_VERIFY_ON_BOOT` 可让上电即复检。若下发参数时 `skip_verify=1`，`DoSave` 落盘后直接打 `calib: verify skipped` 并跳 `S_DONE`（**不**产生 `calib: dvfy` / `verify ch…` / `calib: verify start` 任何复检行）；此时「是否合格」以拟合残差 `s_residualLimit` 为准（超限仍在 `S_FIT` 就 `S_FAIL`，不落盘）。`OP_HOST_VERIFY` 为显式动作，不受 `skip_verify` 影响。
- 运行时参数下发（`OP_HOST_PARAMS`，payload 18 B：`residual_limit/gate/stable_code/settle_ms/first_settle_ms/sample_interval_ms/ref_sample_interval_ms` 各 u16 LE + `stable_count` u8(1..32) + `skip_verify` bit0 + `retry_count` u8(1..9)）：固件回一行 `calib: params gate=%u resid=%u stable=%u settle=%u first=%u samp=%u refsamp=%u n=%u skipv=%u retry=%u`，上位机解析为 `report.fw_params`（「设备已应用」）。**必须在 CONFIG/START/VERIFY/手动 begin-save 之前下发**，运行中收到回 `calib: error busy`（手动采集中 `calib: error manual-busy`）、payload <18 B 回 `calib: error bad-op`。参数不落 Flash，复位回默认；`sample_interval_ms`/`ref_sample_interval_ms`/`residual_limit`/`check_gate` 收到 0 会被置 1。
- `S_DUMP` 导出：每次 tick 发一行、行间 `kLineGapMs=100`，19 行（grid 3 + ch0..3 各 3 + coeff 4）约 1.9 s。**必须节流**——RTT 上行环缓仅 1024 B，一次性发完会被主机读取速率限制而丢字节（见 Runtime/Tooling 的 RTT 条目）。
- 提交前最低要求：该板型 Debug 与 Release 均能构建通过，且 `.hex` 正常生成。
- 发布约定：`README.md` 追加一行版本说明（历史格式 `V1.00 初版, 实现了...`），同步 `CMakeLists.txt` 的 `MAJOR`/`MINOR`。
