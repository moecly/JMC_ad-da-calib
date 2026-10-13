use std::path::Path;

use probe_rs::flashing::{
    download_file_with_options, DownloadOptions, ElfLoader, ElfOptions, HexLoader,
};

/// 把固件文件烧进目标芯片。ELF 走 `ElfLoader`，HEX 走 `HexLoader`。
///
/// 烧录选项保证不清校准扇区 `0x0800F000`：
/// - `do_chip_erase=false`：按扇区擦除而非整片擦除，不动校准记录。
/// - `keep_unwritten_bytes=true`：擦除扇区后回读恢复未写字节，校准记录在扇区内也保留。
/// - `verify=true`：烧后回读校验。
pub fn flash_file(session: &mut probe_rs::Session, path: &Path) -> Result<(), String> {
    let mut options = DownloadOptions::default();
    options.verify = true;
    options.do_chip_erase = false;
    options.keep_unwritten_bytes = true;
    let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("").to_ascii_lowercase();
    let result = match ext.as_str() {
        "hex" => download_file_with_options(session, path, HexLoader, options),
        _ => download_file_with_options(session, path, ElfLoader(ElfOptions::default()), options),
    };
    result.map_err(|e| format!("烧录失败: {e}"))
}
