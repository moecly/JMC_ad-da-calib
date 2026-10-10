chip := "at32f425k8t7"
speed := "10000"
toolchain := "cmake/gcc.cmake"

# 配置：just configure <BOARD> [ROLE] [BUILD_TYPE]
# 每块板/角色用独立 build 目录，避免 CMake cache 冲突
configure board role="APP" type="Debug":
    cmake -DBOARD={{board}} -DCALIB_ROLE={{role}} -DBUILD_TYPE={{type}} \
          -DCMAKE_TOOLCHAIN_FILE={{toolchain}} -G Ninja -B build/{{board}}-{{role}}

# 构建：just build <BOARD> [ROLE] [BUILD_TYPE]
build board role="APP" type="Debug":
    cmake --build build/{{board}}-{{role}}

# 配置并构建：just make <BOARD> [ROLE] [BUILD_TYPE]
make board role="APP" type="Debug":
    just configure {{board}} {{role}} {{type}}
    just build {{board}} {{role}} {{type}}

# 一次配好本轮调试用的两块板
ref_and_dut:
    just make DA_4CH REF
    just make AD_4CH DUT

# 清理某块板的构建目录
clean board role="APP":
    rm -rf build/{{board}}-{{role}}

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

watch board role="APP":
    watchexec -e c,h,cpp,hpp,ld 'cmake --build build/{{board}}-{{role}}'

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

