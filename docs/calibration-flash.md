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
| 4 | 2 | `version` | `2` |
| 6 | 2 | `board` | `1` = AD_4CH，`2` = DA_4CH |
| 8 | 2 | `degree` | `3` |
| 10 | 2 | `channelCount` | `4` |
| 12 | 4 | `payloadLen` | `128` |
| 16 | 2 | `validMask` | 每个 bit = 对应通道系数有效（bit0=ch0 … bit3=ch3），高 12 位必须为 0 |
| 18 | 2 | `reserved` | `0` |
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
- 记录任一字段校验失败（`magic` / `version` / `board` / `degree` / `channelCount` / `validMask` / `crc32`）时，行为与无校准记录一致：按原值直通。
- `version` 必须为 `2`：v1 记录（偏移 16 原为 4 字节 `reserved`）会被 Load 拒绝，从而按无记录处理（原值直通）。
- 运行时按 `validMask` 逐通道回退：仅当 `degree` 匹配且 `validMask` 对应 bit 置位时才用该通道系数换算，否则该通道原值直通（AD 侧 `ideal=raw`、DA 侧 `raw=ideal`）。因此单通道校准只写回被标定通道的 bit，其余通道系数保持原值但不再被使用。
- 不再存在硬编码内建曲线：校准系数一律来自 Flash 记录，新板须先经手动校准写入。

拟合残差门限 `s_residualLimit`（`CalibProc.cpp` 的 `kDefResidualLimit`，默认 **100** 码；自 v1.9 起为运行时参数，可由上位机经 `OP_HOST_PARAMS` 覆盖，不落 Flash），超限则本次校准失败、不写 Flash。取 100 的依据：单路正确接法下实测 AD `resid max=1.5 / rmse=0.5`、DA 方向 ~14 码，留 ~7× 余量；而接线错误（多路共接）时残差达 28875 码（≈44% FS），仍被挡住。历史值 1000 只是为放开落盘而放宽的兜底——当时观察到实测残差 130~170 码，判断为施加端/公共端的物理层漂移而非拟合/量化误差：同一 ideal 下扫描期与复检期 raw 相差恒定 ~160 码（≈2400 ppm）、符号随机、成对反号且配对跨数千码，与建压时间（200/600 ms）、盒式窗深度（32/4）、通道轮转（4 ms/单通道连发）均无关；把 ideal 钉死在单点（DA 只写一次）时残差降至 ~43。低段 `pt0..6` 仅 1 码，证明 SPI 帧、通道映射、盒式均值、Horner 求值全部正确。详见 `AGENTS.md` 的 Testing & QA。

## CRC-32

标准 CRC-32（CRC-32/ISO-HDLC，即 zlib/PNG 使用的算法）：反射多项式 `0xEDB88320`，初值 `0xFFFFFFFF`，结果与 `0xFFFFFFFF` 异或。实现见 `user/CRC/CRC.cpp`（`CRCCheck()` 默认参数）。校验值：`CRC32("123456789") == 0xCBF43926`。

## 读取与解码示例

`probe-rs read` 仅支持 RAM 地址，读取 Flash 内容需用调试器导出该扇区（如 J-Link `savebin 0x0800F000 152 dump.bin`），再用下列脚本解码：

```python
import struct, zlib

d = open('dump.bin', 'rb').read()          # 0x0800F000 起 152 字节
magic, ver, board, deg, nch, plen = struct.unpack_from('<IHHHHI', d, 0)
valid_mask = struct.unpack_from('<H', d, 16)[0]
crc = struct.unpack_from('<I', d, 148)[0]
print(hex(magic), ver, board, deg, nch, plen, hex(valid_mask), hex(crc))
assert magic == 0x314C4143 and ver == 2 and deg == 3 and nch == 4 and plen == 128
assert valid_mask & ~0xF == 0
assert crc == zlib.crc32(d[:148])

for ch in range(4):
    print(ch, struct.unpack_from('<4d', d, 20 + 32 * ch))
```
