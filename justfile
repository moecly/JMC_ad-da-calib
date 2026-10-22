chip := "at32f425k8t7"
speed := "10000"
toolchain := "cmake/gcc.cmake"

# 板型（以 Board/ 实际目录为准）
BOARDS := "AD_4CH DA_4CH"
# 校准角色；APP 固件不在本仓库本地脚本编译
ROLES := "REF DUT MANUAL"
TYPES := "Debug Release"
# 发布包需要 APP 固件；本地调试默认只编 REF/DUT/MANUAL
RELEASE_ROLES := "APP REF DUT MANUAL"

# 配置单个组合：just configure <BOARD> <ROLE> [TYPE]
# 目录 build/<BOARD>_<ROLE>_<TYPE>
configure board role type="Debug":
    cmake -DBOARD={{board}} -DCALIB_ROLE={{role}} -DBUILD_TYPE={{type}} \
          -DCMAKE_TOOLCHAIN_FILE={{toolchain}} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G Ninja \
          -S . -B "build/{{board}}_{{role}}_{{type}}"

# 并发配置所有 板型 × 角色 × 类型（Debug / Release 一起）
configure_all:
    @for t in {{TYPES}}; do \
        for r in {{ROLES}}; do \
            for b in {{BOARDS}}; do \
                cmake -DBOARD=$b -DCALIB_ROLE=$r -DBUILD_TYPE=$t \
                    -DCMAKE_TOOLCHAIN_FILE={{toolchain}} \
                    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G Ninja -S . \
                    -B "build/${b}_${r}_${t}" & \
            done; \
        done; \
    done; wait

# 并发构建 build/ 下所有已配置目录
build:
    for d in build/*/; do \
        [ -f "$d/build.ninja" ] && cmake --build "$d" & \
    done; wait

# 全量：并发配置 + 并发构建
all: configure_all build

# 全量固件（含 APP）× Debug/Release，打包到 out/firmware-v<MAJOR>.<MINOR>.zip
release:
    @set -eu; \
    major=$(sed -n 's/^set(MAJOR \([0-9]\+\))/\1/p' CMakeLists.txt); \
    minor=$(sed -n 's/^set(MINOR \([0-9]\+\))/\1/p' CMakeLists.txt); \
    ver="$major.$minor"; \
    rm -rf out/fw-build out/fw-dist out/firmware-v$ver.zip out/firmware-v$ver.zip.sha256; \
    mkdir -p out/fw-build out/fw-dist; \
    for t in {{TYPES}}; do \
      for r in {{RELEASE_ROLES}}; do \
        for b in {{BOARDS}}; do \
          cmake -DBOARD=$b -DCALIB_ROLE=$r -DBUILD_TYPE=$t \
                -DCMAKE_TOOLCHAIN_FILE={{toolchain}} -G Ninja -S . \
                -B "out/fw-build/${b}-${r}-${t}" >/dev/null; \
        done; \
      done; \
    done; \
    for d in out/fw-build/*/; do \
      cmake --build "$d"; \
    done; \
    find out/fw-build -maxdepth 2 -type f \( -name '*.elf' -o -name '*.hex' -o -name '*.map' \) -exec cp {} out/fw-dist/ \;; \
    cd out/fw-dist; \
    python3 -c 'import os,sys,zipfile; z=zipfile.ZipFile(sys.argv[1],"w",zipfile.ZIP_DEFLATED); [z.write(f,f) for f in sorted(os.listdir("."))]; z.close()' "../firmware-v${ver}.zip"; \
    cd ..; \
    sha256sum "firmware-v${ver}.zip" > "firmware-v${ver}.zip.sha256"; \
    ls -lh "firmware-v${ver}.zip" "firmware-v${ver}.zip.sha256"

# 配置并构建单个组合：just make <BOARD> <ROLE> [TYPE]
make board role type="Debug":
    just configure {{board}} {{role}} {{type}}
    cmake --build "build/{{board}}_{{role}}_{{type}}"

# 一次配好本轮调试用的两块板
ref_and_dut:
    just make DA_4CH REF
    just make AD_4CH DUT

clean:
    rm -rf build

clean_rebuild: clean all

download_and_reset elf:
    just download {{elf}} && just reset

# 烧录并运行（日志走 RTT，直接打在终端；ELF 被 -s 去符号，须扫内存找控制块）
run elf:
    probe-rs run --chip {{chip}} --preverify --speed {{speed}} --rtt-scan-memory {{elf}}

# 只烧录、只连调试器看日志（不下载），Ctrl-C 退出
attach elf:
    probe-rs attach --chip {{chip}} --rtt-scan-memory {{elf}}

# 只烧录不运行
download elf:
    probe-rs download --chip {{chip}} --preverify --speed {{speed}} {{elf}}

# 擦除芯片
erase:
    probe-rs erase --chip {{chip}} --speed {{speed}}

# 复位芯片
reset:
    probe-rs reset --chip {{chip}}

# 查看芯片信息
info:
    probe-rs info --chip {{chip}}

watch board role type="Debug":
    watchexec -e c,h,cpp,hpp,ld 'cmake --build build/{{board}}_{{role}}_{{type}}'

# 上位机（host/，Tauri）：先 git add host/ 让嵌套 flake 可见，再进 host 的 nix shell
# 安装前端依赖
host-install:
    cd host && nix develop --command npm install

# 启动桌面应用（RTT 双向控制：下行命令 + 上行校准日志）
host:
    cd host && nix develop --command npm run tauri dev

# 前端构建（tsc + vite）
host-build:
    cd host && nix develop --command npm run build

# 日志解析器单测（无需硬件）
host-test:
    cd host && nix develop --command cargo test --manifest-path src-tauri/Cargo.toml

# 产线烧录校准上位机（prod/，Tauri）：先 git add prod/ 让嵌套 flake 可见，再进 prod 的 nix shell
# 安装前端依赖
prod-install:
    cd prod && nix develop --command npm install

# 启动桌面应用（自动化流水：烧录 DUT → 自动校准 → 复检 PASS 烧 APP → 校验启动日志）
prod:
    cd prod && nix develop --command npm run tauri dev

# 前端构建（tsc + vite）
prod-build:
    cd prod && nix develop --command npm run build

# 后端构建
prod-build-backend:
    cd prod && nix develop --command cargo build --manifest-path src-tauri/Cargo.toml

# 纯函数单测（无需硬件；verify_log_pass / calib_pass 等）
prod-test:
    cd prod && nix develop --command cargo test --manifest-path src-tauri/Cargo.toml

