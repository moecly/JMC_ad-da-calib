chip := "at32f425k8t7"
speed := "10000"

download_and_reset elf:
    just download {{elf}} && just reset

# 烧录并运行
run elf:
    probe-rs run --chip {{chip}} --preverify --speed {{speed}} {{elf}}

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

watch:
    watchexec -e c,h,cpp,hpp,ld 'cmake --build build'

