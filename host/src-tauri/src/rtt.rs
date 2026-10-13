use std::collections::VecDeque;
use std::sync::atomic::{AtomicBool, AtomicU8, Ordering};
use std::sync::{Arc, LazyLock};
use std::thread::sleep;
use std::time::Duration;

use parking_lot::Mutex;
use probe_rs::probe::list::Lister;
use probe_rs::rtt::{Rtt, ScanRegion};
use probe_rs::{CoreStatus, Permissions};
use regex::Regex;
use serde::Serialize;
use tauri::{AppHandle, Emitter};

use crate::calib::Parser;

pub const MAX_LINES: usize = 5000;
const PENDING_MAX: usize = 4096;
const ATTACH_MAX_TRIES: u32 = 40;
const PROBE_LOST_HINT: &str = "重新插好 J-Link 后点『连接』重建会话（拔插时会话已释放，不会占用探针）";
// 控制块只在固件 startup 由 .data 拷贝写进 SRAM：目标若被停机在复位态，SRAM 里没有
// "SEGGER RTT" 魔数，单纯重扫永远不会成功，必须周期性强制复位并运行让固件重建控制块
const AUTO_RESET_AFTER: u32 = 3;

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
    pub state: String,
    pub message: String,
    pub hint: Option<String>,
    pub attempts: u32,
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
    control: Arc<AtomicU8>,
    probe_index: usize,
    chip: String,
    sink: Arc<Mutex<VecDeque<LogLine>>>,
    parser: Arc<Mutex<Parser>>,
    status: Arc<Mutex<Status>>,
    commands: Arc<Mutex<VecDeque<Vec<u8>>>>,
) -> std::thread::JoinHandle<()> {
    std::thread::spawn(move || {
        worker_loop(app, stop, control, probe_index, chip, sink, parser, status, commands)
    })
}

fn mkstatus(connected: bool, state: &str, message: impl Into<String>, hint: Option<String>, attempts: u32) -> Status {
    Status {
        connected,
        state: state.to_string(),
        message: message.into(),
        hint,
        attempts,
    }
}

pub(crate) fn emit_status(app: &AppHandle, shared: &Arc<Mutex<Status>>, status: Status) {
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

/// 探针掉线（拔 J-Link）时 probe-rs 的 RTT 扫描会把内存读错误当成「没找到控制块」
/// （`rtt::find_control_block` 内 `core.read(..).ok()?`），错误一律是 ControlBlockNotFound，
/// 与「目标没跑到 RTT 初始化」无法从错误类型区分；只能另发一次真实内存访问来判定探针是否还在
fn probe_io_error(core: &mut probe_rs::Core<'_>) -> Option<String> {
    core.status().err().map(|e| e.to_string())
}

fn worker_loop(
    app: AppHandle,
    stop: Arc<AtomicBool>,
    control: Arc<AtomicU8>,
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
            emit_status(&app, &status, mkstatus(false, "error", format!("未找到探针 #{probe_index}"), None, 0));
            return;
        }
    };
    let probe = match info.open() {
        Ok(p) => p,
        Err(e) => {
            emit_status(&app, &status, mkstatus(false, "error", format!("打开探针失败: {e}"), None, 0));
            return;
        }
    };
    let mut session = match probe.attach(chip.as_str(), Permissions::default()) {
        Ok(s) => s,
        Err(e) => {
            emit_status(&app, &status, mkstatus(false, "error", format!("附着芯片 {chip} 失败: {e}"), None, 0));
            return;
        }
    };
    let mut core = match session.core(0) {
        Ok(c) => c,
        Err(e) => {
            emit_status(&app, &status, mkstatus(false, "error", format!("选择核心失败: {e}"), None, 0));
            return;
        }
    };

    // 附着后目标可能停在 halt 态，固件不跑就不会初始化 RTT，`Rtt::attach` 永远失败
    match core.status() {
        Ok(s) => {
            if !matches!(s, CoreStatus::Running) {
                if let Err(e) = core.run() {
                    emit_status(
                        &app,
                        &status,
                        mkstatus(false, "error", format!("目标处于 {s:?}，恢复运行失败: {e}"), None, 0),
                    );
                    return;
                }
                emit_status(
                    &app,
                    &status,
                    mkstatus(false, "connecting", format!("目标之前处于 {s:?}，已恢复运行"), None, 0),
                );
            }
        }
        Err(e) => {
            emit_status(&app, &status, mkstatus(false, "error", format!("读取内核状态失败: {e}"), None, 0));
            return;
        }
    }

    let mut tries = 0u32;
    while !stop.load(Ordering::Relaxed) {
        match control.swap(0, Ordering::SeqCst) {
            1 => {
                match core.reset() {
                    Ok(()) => emit_status(&app, &status, mkstatus(false, "connecting", "已复位并运行", None, 0)),
                    Err(e) => emit_status(
                        &app,
                        &status,
                        mkstatus(false, "error", format!("复位失败: {e}"), None, 0),
                    ),
                }
                continue;
            }
            2 => {
                match core.run() {
                    Ok(()) => emit_status(&app, &status, mkstatus(false, "connecting", "已恢复运行", None, 0)),
                    Err(e) => emit_status(
                        &app,
                        &status,
                        mkstatus(false, "error", format!("恢复运行失败: {e}"), None, 0),
                    ),
                }
                continue;
            }
            _ => {}
        }
        let rtt = Rtt::attach(&mut core).or_else(|_| {
            Rtt::attach_region(&mut core, &ScanRegion::range(0x2000_0000..0x2000_5000))
        });
        let mut rtt = match rtt {
            Ok(r) => r,
            Err(e) => {
                if let Some(pe) = probe_io_error(&mut core) {
                    emit_status(
                        &app,
                        &status,
                        mkstatus(
                            false,
                            "error",
                            format!("探针通信中断（J-Link 已断开？）: {pe}"),
                            Some(PROBE_LOST_HINT.to_string()),
                            tries,
                        ),
                    );
                    // 必须退出线程：否则 conn 一直持有死会话，界面显示「连接」而点击回「已连接」
                    return;
                }
                tries += 1;
                if tries == AUTO_RESET_AFTER || (tries > AUTO_RESET_AFTER && tries % 10 == 0) {
                    let _ = core.reset();
                }
                if tries >= ATTACH_MAX_TRIES {
                    emit_status(
                        &app,
                        &status,
                        mkstatus(
                            false,
                            "error",
                            format!("未找到 RTT 控制块（已重试 {ATTACH_MAX_TRIES} 次仍失败，已停止）: {e}"),
                            // 会话已释放，『复位并运行』不可用；重连会自动复位运行
                            Some("点『连接』重开会话会自动复位运行；仍失败则给板子断电重上电".to_string()),
                            tries,
                        ),
                    );
                    // 同上：停手并释放会话，否则「连接」按钮永远点不动
                    return;
                }
                emit_status(
                    &app,
                    &status,
                    mkstatus(
                        false,
                        "searching",
                        format!("未找到 RTT 控制块（第 {tries} 次）: {e}"),
                        Some(hint_for(&e)),
                        tries,
                    ),
                );
                sleep(Duration::from_millis(500));
                continue;
            }
        };
        tries = 0;
        emit_status(
            &app,
            &status,
            mkstatus(true, "attached", format!("已连接 {}", info.identifier), None, 0),
        );
        match read_loop(&app, &stop, &control, &mut core, &mut rtt, &sink, &parser, &commands) {
            Ok(()) => {}
            Err(ReadStop::Failed(msg)) => {
                emit_status(&app, &status, mkstatus(false, "error", msg, None, 0));
                sleep(Duration::from_millis(500));
            }
            Err(ReadStop::Reattach) => {}
        }
    }
}

enum ReadStop {
    Failed(String),
    Reattach,
}

fn read_loop(
    app: &AppHandle,
    stop: &Arc<AtomicBool>,
    control: &Arc<AtomicU8>,
    core: &mut probe_rs::Core<'_>,
    rtt: &mut Rtt,
    sink: &Arc<Mutex<VecDeque<LogLine>>>,
    parser: &Arc<Mutex<Parser>>,
    commands: &Arc<Mutex<VecDeque<Vec<u8>>>>,
) -> Result<(), ReadStop> {
    let mut buf = [0u8; 1024];
    let mut pending: Vec<u8> = Vec::with_capacity(PENDING_MAX);
    while !stop.load(Ordering::Relaxed) {
        match control.swap(0, Ordering::SeqCst) {
            1 => {
                if let Err(e) = core.reset() {
                    return Err(ReadStop::Failed(format!("复位失败: {e}")));
                }
                return Err(ReadStop::Reattach);
            }
            2 => {
                if let Err(e) = core.run() {
                    return Err(ReadStop::Failed(format!("恢复运行失败: {e}")));
                }
                return Err(ReadStop::Reattach);
            }
            _ => {}
        }
        // down_channel/up_channel 均为 &mut self，不能同时持有，故每轮分别借用
        if let Some(down) = rtt.down_channel(0) {
            loop {
                let frame = { commands.lock().pop_front() };
                match frame {
                    Some(f) => {
                        down.write(core, &f).map_err(|e| ReadStop::Failed(e.to_string()))?;
                    }
                    None => break,
                }
            }
        }
        let channel = match rtt.up_channel(0) {
            Some(c) => c,
            None => return Err(ReadStop::Failed("未找到上行通道 0".to_string())),
        };
        let n = channel.read(core, &mut buf).map_err(|e| ReadStop::Failed(e.to_string()))?;
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

fn hint_for(e: &probe_rs::rtt::Error) -> String {
    use probe_rs::rtt::Error as RttError;
    match e {
        RttError::ControlBlockNotFound => {
            "目标可能被复位/停机且固件未跑到 RTT 初始化；点『复位并运行』，或给板子断电重上电后重连".to_string()
        }
        RttError::MultipleControlBlocksFound(_) => {
            "内存中有多个 RTT 控制块（多次复位残留），请『复位并运行』".to_string()
        }
        RttError::ControlBlockCorrupted(_) => "RTT 控制块损坏，请复位目标或重烧固件".to_string(),
        _ => format!("探针/内存读取异常：{e}"),
    }
}
