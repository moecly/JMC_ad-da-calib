use std::sync::LazyLock;

use probe_rs::probe::list::Lister;
use regex::Regex;
use serde::Serialize;

#[derive(Clone, Serialize)]
pub struct ProbeInfo {
    pub index: usize,
    pub name: String,
    pub serial: Option<String>,
}

#[derive(Clone, Serialize)]
pub struct LogLine {
    pub level: String,
    pub file: String,
    pub line: u32,
    pub func: String,
    pub message: String,
    pub raw: String,
}

pub fn list_probes() -> Result<Vec<ProbeInfo>, String> {
    let probes = Lister::new().list_all();
    Ok(probes
        .into_iter()
        .enumerate()
        .map(|(index, info)| ProbeInfo {
            index,
            name: info.identifier.clone(),
            serial: info.serial_number.clone(),
        })
        .collect())
}

static LINE_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^\[(\w+)\]\s+(\S+):(\d+)\s+\(([^)]*)\):\s?(.*)$").expect("valid line regex")
});

pub(crate) fn parse_line(raw: &str) -> LogLine {
    if let Some(caps) = LINE_RE.captures(raw) {
        LogLine {
            level: caps[1].to_string(),
            file: caps[2].to_string(),
            line: caps[3].parse().unwrap_or(0),
            func: caps[4].to_string(),
            message: caps[5].to_string(),
            raw: raw.to_string(),
        }
    } else {
        LogLine {
            level: String::new(),
            file: String::new(),
            line: 0,
            func: String::new(),
            message: raw.to_string(),
            raw: raw.to_string(),
        }
    }
}

/// RTT 附着失败的排障建议：控制块只在固件 startup 由 .data 拷贝写进 SRAM，
/// 「找不到控制块」多半是目标停在复位态而非内存里真的没有。
pub(crate) fn hint_for(e: &probe_rs::rtt::Error) -> String {
    use probe_rs::rtt::Error as RttError;
    match e {
        RttError::ControlBlockNotFound => {
            "目标可能被复位/停机且固件未跑到 RTT 初始化，请给板子断电重上电后重试".to_string()
        }
        RttError::MultipleControlBlocksFound(_) => {
            "内存中有多个 RTT 控制块（多次复位残留），请给板子断电重上电后重试".to_string()
        }
        RttError::ControlBlockCorrupted(_) => "RTT 控制块损坏，请重烧固件".to_string(),
        _ => format!("探针/内存读取异常：{e}"),
    }
}
