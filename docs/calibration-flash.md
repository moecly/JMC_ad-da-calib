# 校准系数 Flash 存储格式

校准程序（`calib/`）把 4 通道三次多项式系数写入 Flash 最后一页，供应用运行时读取。

## 存储位置

| 项目 | 值 |
|---|---|
| 记录基址 `CALIB_BASE` | `0x0800F000` |
| 占用范围 | `0x0800F000` – `0x0800F7FF`（1 个 2 KB 扇区） |
| 记录长度 | 152 字节（其余字节为擦除态 `0xFF`） |
| 应用上限 | 60 KB（`0x08000000` – `0x0800EFFF`，见 `ldscripts/AT32F425x8_FLASH.ld` 的 `LENGTH = 60K`） |

写入流程：`flash_unlock()` → `flash_sector_erase(0x0800F000)` → 38 次 `flash_word_program()`（每次 4 字节，逐字等待）→ `flash_lock()`，随后整条记录回读比对。

## 字节布局

小端。所有多字节字段按 `memcpy` 组包，无结构体对齐填充。

| 偏移 | 长度 | 字段 | 取值 |
|---|---|---|---|
| 0 | 4 | `magic` | `0x314C4143` |
| 4 | 2 | `version` | `1` |
| 6 | 2 | `board` | `1` = AD_4CH，`2` = DA_4CH |
| 8 | 2 | `degree` | `3` |
| 10 | 2 | `channelCount` | `4` |
| 12 | 4 | `payloadLen` | `128` |
| 16 | 4 | `reserved` | `0` |
| 20 | 128 | `coeff[4][4]` | 4 通道 × 4 个 IEEE-754 binary64 |
| 148 | 4 | `crc32` | 对字节 `[0,148)` 的 CRC-32 |
| 152 | — | 记录末尾 | — |

`coeff[ch][k]`：通道 `ch` 的第 `k` 次幂系数，按 Horner 求值。

## 系数语义

`board` 决定多项式方向，两者都是「输入码 → 输出码」，输入/输出均为 `[0,65535]` 的 ADC/DAC 码。

| board | 板型 | 多项式 | 输入 | 输出 |
|---|---|---|---|---|
| 1 | AD_4CH | `ideal = f(raw)` | 原始 ADC 码 | 标定后理想码 |
| 2 | DA_4CH | `raw = f(ideal)` | 理想码 | 写入 DAC 的原始码 |

取值契约：

- AD 侧 `CalibAdIdeal()` 直接返回 `PolyEval()` 结果（`float`），**不钳位、不取整**。
- DA 侧 `CalibDaRaw()` 返回 `uint16_t`：结果先钳位到 `[0,65535]`，再 `+0.5` 截断取整。
- 记录任一字段校验失败（`magic` / `version` / `board` / `degree` / `channelCount` / `crc32`）时，回退到各 `Board.cpp` 内硬编码的 `code_to_code()`，行为与无校准记录一致。

拟合残差门限：`32` 码（`CalibProc.cpp` 的 `kResidualLimitCode`），超限则本次校准失败、不写 Flash。

## CRC-32

标准 CRC-32（CRC-32/ISO-HDLC，即 zlib/PNG 使用的算法）：反射多项式 `0xEDB88320`，初值 `0xFFFFFFFF`，结果与 `0xFFFFFFFF` 异或。实现见 `user/CRC/CRC.cpp`（`CRCCheck()` 默认参数）。校验值：`CRC32("123456789") == 0xCBF43926`。

## 读取与解码示例

`probe-rs read` 仅支持 RAM 地址，读取 Flash 内容需用调试器导出该扇区（如 J-Link `savebin 0x0800F000 152 dump.bin`），再用下列脚本解码：

```python
import struct, zlib

d = open('dump.bin', 'rb').read()          # 0x0800F000 起 152 字节
magic, ver, board, deg, nch, plen = struct.unpack_from('<IHHHHI', d, 0)
crc = struct.unpack_from('<I', d, 148)[0]
print(hex(magic), ver, board, deg, nch, plen, hex(crc))
assert magic == 0x314C4143 and ver == 1 and deg == 3 and nch == 4 and plen == 128
assert crc == zlib.crc32(d[:148])

for ch in range(4):
    print(ch, struct.unpack_from('<4d', d, 20 + 32 * ch))
```
