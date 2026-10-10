use std::collections::VecDeque;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, LazyLock};
use std::thread::sleep;
use std::time::Duration;

use parking_lot::Mutex;
use probe_rs::probe::list::Lister;
use probe_rs::rtt::{Rtt, ScanRegion};
use probe_rs::Permissions;
use regex::Regex;
use serde::Serialize;
use tauri::{AppHandle, Emitter};

use crate::calib::Parser;

pub const MAX_LINES: usize = 5000;
const PENDING_MAX: usize = 4096;
const ATTACH_MAX_TRIES: u32 = 40;

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

#[derive(Clone, Default, Serialize)]
pub struct Status {
    pub connected: bool,
    pub message: String,
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

pub fn spawn_worker(
    app: AppHandle,
    stop: Arc<AtomicBool>,
    probe_index: usize,
    chip: String,
    sink: Arc<Mutex<VecDeque<LogLine>>>,
    parser: Arc<Mutex<Parser>>,
    status: Arc<Mutex<Status>>,
    commands: Arc<Mutex<VecDeque<Vec<u8>>>>,
) -> std::thread::JoinHandle<()> {
    std::thread::spawn(move || worker_loop(app, stop, probe_index, chip, sink, parser, status, commands))
}

pub(crate) fn emit_status(
    app: &AppHandle,
    shared: &Arc<Mutex<Status>>,
    connected: bool,
    message: impl Into<String>,
) {
    let status = Status {
        connected,
        message: message.into(),
    };
    *shared.lock() = status.clone();
    let _ = app.emit("rtt://status", status);
}

pub(crate) fn process_line(
    app: &AppHandle,
    sink: &Arc<Mutex<VecDeque<LogLine>>>,
    parser: &Arc<Mutex<Parser>>,
    raw: &str,
) {
    let line = parse_line(raw);
    let _ = app.emit("rtt://line", &line);
    {
        let mut q = sink.lock();
        if q.len() >= MAX_LINES {
            q.pop_front();
        }
        q.push_back(line.clone());
    }
    let updated = {
        let mut p = parser.lock();
        if p.feed(&line.message) {
            Some(p.report.clone())
        } else {
            None
        }
    };
    if let Some(report) = updated {
        let _ = app.emit("calib://report", report);
    }
}

fn worker_loop(
    app: AppHandle,
    stop: Arc<AtomicBool>,
    probe_index: usize,
    chip: String,
    sink: Arc<Mutex<VecDeque<LogLine>>>,
    parser: Arc<Mutex<Parser>>,
    status: Arc<Mutex<Status>>,
    commands: Arc<Mutex<VecDeque<Vec<u8>>>>,
) {
    let info = match Lister::new().list_all().into_iter().nth(probe_index) {
        Some(i) => i,
        None => {
            emit_status(&app, &status, false, format!("未找到探针 #{probe_index}"));
            return;
        }
    };
    let probe = match info.open() {
        Ok(p) => p,
        Err(e) => {
            emit_status(&app, &status, false, format!("打开探针失败: {e}"));
            return;
        }
    };
    let mut session = match probe.attach(chip.as_str(), Permissions::default()) {
        Ok(s) => s,
        Err(e) => {
            emit_status(&app, &status, false, format!("附着芯片 {chip} 失败: {e}"));
            return;
        }
    };
    let mut core = match session.core(0) {
        Ok(c) => c,
        Err(e) => {
            emit_status(&app, &status, false, format!("选择核心失败: {e}"));
            return;
        }
    };

    let mut tries = 0u32;
    while !stop.load(Ordering::Relaxed) {
        let rtt = Rtt::attach(&mut core).or_else(|_| {
            Rtt::attach_region(&mut core, &ScanRegion::range(0x2000_0000..0x2000_5000))
        });
        let mut rtt = match rtt {
            Ok(r) => r,
            Err(e) => {
                tries += 1;
                if tries >= ATTACH_MAX_TRIES {
                    emit_status(&app, &status, false, format!("未找到 RTT 控制块，继续重试: {e}"));
                    tries = 0;
                }
                sleep(Duration::from_millis(500));
                continue;
            }
        };
        tries = 0;
        emit_status(&app, &status, true, format!("已连接 {}", info.identifier));
        if let Err(msg) = read_loop(&app, &stop, &mut core, &mut rtt, &sink, &parser, &commands) {
            emit_status(&app, &status, false, msg);
            sleep(Duration::from_millis(500));
        }
    }
}

fn read_loop(
    app: &AppHandle,
    stop: &Arc<AtomicBool>,
    core: &mut probe_rs::Core<'_>,
    rtt: &mut Rtt,
    sink: &Arc<Mutex<VecDeque<LogLine>>>,
    parser: &Arc<Mutex<Parser>>,
    commands: &Arc<Mutex<VecDeque<Vec<u8>>>>,
) -> Result<(), String> {
    let mut buf = [0u8; 1024];
    let mut pending: Vec<u8> = Vec::with_capacity(PENDING_MAX);
    while !stop.load(Ordering::Relaxed) {
        // down_channel/up_channel 均为 &mut self，不能同时持有，故每轮分别借用
        if let Some(down) = rtt.down_channel(0) {
            loop {
                let frame = { commands.lock().pop_front() };
                match frame {
                    Some(f) => {
                        down.write(core, &f).map_err(|e| e.to_string())?;
                    }
                    None => break,
                }
            }
        }
        let channel = match rtt.up_channel(0) {
            Some(c) => c,
            None => return Err("未找到上行通道 0".to_string()),
        };
        let n = channel.read(core, &mut buf).map_err(|e| e.to_string())?;
        if n == 0 {
            sleep(Duration::from_millis(10));
            continue;
        }
        pending.extend_from_slice(&buf[..n]);
        while let Some(pos) = pending.iter().position(|&b| b == b'\n') {
            let mut one: Vec<u8> = pending.drain(..=pos).collect();
            one.pop();
            if one.last() == Some(&b'\r') {
                one.pop();
            }
            let text = String::from_utf8_lossy(&one);
            process_line(app, sink, parser, &text);
        }
        if pending.len() > PENDING_MAX {
            pending.clear();
        }
    }
    Ok(())
}
