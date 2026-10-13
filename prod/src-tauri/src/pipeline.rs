use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread::sleep;
use std::time::{Duration, SystemTime};

use parking_lot::{Condvar, Mutex};
use probe_rs::probe::list::Lister;
use probe_rs::rtt::{Rtt, ScanRegion};
use probe_rs::Permissions;
use serde::{Deserialize, Serialize};
use tauri::{AppHandle, Emitter};

use crate::calib::{CalibReport, Parser};
use crate::flash;
use crate::rtt::{parse_line, LogLine};

pub const CALIB_TIMEOUT_SECS: u64 = 600;
pub const VERIFY_LOG_LINES: usize = 80;
pub const VERIFY_LOG_TIMEOUT_MS: u64 = 10_000;
const ATTACH_MAX_TRIES: u32 = 40;
const AUTO_RESET_AFTER: u32 = 3;
const MAX_LINES: usize = 5000;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PipelineConfig {
    pub probe_index: usize,
    pub chip: String,
    pub dut_path: PathBuf,
    pub app_path: PathBuf,
    pub ch_mask: u8,
    pub ref_da: [u8; 4],
    pub auto_next: bool,
}

#[derive(Clone, Serialize)]
pub struct PipelineStatus {
    pub step: String,
    pub ok: Option<bool>,
    pub message: String,
    pub log_lines: Vec<LogLine>,
    pub report: CalibReport,
    pub running: bool,
}

impl Default for PipelineStatus {
    fn default() -> Self {
        Self {
            step: "idle".to_string(),
            ok: None,
            message: String::new(),
            log_lines: Vec::new(),
            report: CalibReport::default(),
            running: false,
        }
    }
}

#[derive(Default)]
pub struct PipelineShared {
    pub status: Mutex<PipelineStatus>,
    pub next_gate: (Mutex<bool>, Condvar),
}

/// 启动校验日志是否通过：固件上电打印 `Device: <板型>, App Version: vX.Y` 与 `Init success`。
/// 两个信号都必须出现。
pub fn verify_log_pass(lines: &[String]) -> bool {
    let has_device =
        lines.iter().any(|l| l.contains("Device:") && l.contains("App Version:"));
    let has_init = lines.iter().any(|l| l.contains("Init success"));
    has_device && has_init
}

/// 校准是否通过：以固件复检结论 `calib: verify PASS/FAIL` 为准（解析器写入 `verify_pass`）。
pub fn calib_pass(report: &CalibReport) -> Option<bool> {
    report.verify_pass
}

pub fn spawn_pipeline(
    app: AppHandle,
    cfg: PipelineConfig,
    stop: Arc<AtomicBool>,
    shared: Arc<PipelineShared>,
) -> std::thread::JoinHandle<()> {
    std::thread::spawn(move || pipeline_loop(app, cfg, stop, shared))
}

/// 推送流程状态。payload 不带 log_lines：日志已由 `prod://line` 事件单独推送，
/// 若把 log_lines（可累积到 MAX_LINES 数千行）一并克隆/序列化，主线程会被拖住导致 UI 卡死。
pub fn emit_status(app: &AppHandle, shared: &PipelineShared) {
    let s = shared.status.lock();
    let ev = PipelineStatus {
        step: s.step.clone(),
        ok: s.ok,
        message: s.message.clone(),
        log_lines: Vec::new(),
        report: s.report.clone(),
        running: s.running,
    };
    drop(s);
    let _ = app.emit("prod://status", ev);
}

fn set_status(shared: &PipelineShared, step: &str, ok: Option<bool>, message: impl Into<String>) {
    let mut s = shared.status.lock();
    s.step = step.to_string();
    s.ok = ok;
    s.message = message.into();
}

fn append_line(shared: &PipelineShared, app: &AppHandle, line: LogLine) {
    {
        let mut s = shared.status.lock();
        if s.log_lines.len() >= MAX_LINES {
            s.log_lines.remove(0);
        }
        s.log_lines.push(line.clone());
    }
    let _ = app.emit("prod://line", line);
}

fn push_report(shared: &PipelineShared, app: &AppHandle, report: &CalibReport) {
    shared.status.lock().report = report.clone();
    let _ = app.emit("prod://report", report);
}

fn open_session(cfg: &PipelineConfig) -> Result<probe_rs::Session, String> {
    let info = Lister::new()
        .list_all()
        .into_iter()
        .nth(cfg.probe_index)
        .ok_or_else(|| format!("未找到探针 #{}", cfg.probe_index))?;
    let probe = info.open().map_err(|e| format!("打开探针失败: {e}"))?;
    probe.attach(cfg.chip.as_str(), Permissions::default()).map_err(|e| format!("附着芯片 {} 失败: {e}", cfg.chip))
}

/// 附着到目标并确保固件运行，循环找 RTT 控制块（沿用 rtt.rs 的复位/运行重试策略）。
fn attach_rtt(
    app: &AppHandle,
    shared: &Arc<PipelineShared>,
    stop: &Arc<AtomicBool>,
    core: &mut probe_rs::Core<'_>,
) -> Result<Rtt, String> {
    // 烧录（probe-rs 默认 ResetAndHalt）后目标停在 halt 态，且烧录不清 SRAM，
    // 会残留上一次运行的 RTT 控制块。必须从复位态重新启动固件，重建控制块并覆盖残留，
    // 否则 Rtt::attach 命中残留块，其上行通道无当前固件写入，读恒 0。
    core.reset().map_err(|e| format!("复位目标失败: {e}"))?;
    core.run().map_err(|e| format!("恢复运行失败: {e}"))?;
    let mut tries = 0u32;
    loop {
        if stop.load(Ordering::Relaxed) {
            return Err("已停止".to_string());
        }
        let rtt = Rtt::attach(core).or_else(|_| {
            Rtt::attach_region(core, &ScanRegion::range(0x2000_0000..0x2000_5000))
        });
        match rtt {
            Ok(r) => return Ok(r),
            Err(e) => {
                tries += 1;
                if tries == AUTO_RESET_AFTER || (tries > AUTO_RESET_AFTER && tries % 10 == 0) {
                    let _ = core.reset();
                    let _ = core.run(); // reset 为 halt 语义，须 run 恢复，否则固件永不初始化 RTT
                }
                set_status(shared, "attach", None, format!("未找到 RTT 控制块（第 {tries} 次）: {e}"));
                emit_status(&app, &shared);
                sleep(Duration::from_millis(500));
                if tries >= ATTACH_MAX_TRIES {
                    return Err(format!("未找到 RTT 控制块: {e}"));
                }
            }
        }
    }
}

/// 执行校准：下发改定 + 启动帧，读上行喂解析器直到复检结论出现或超时。
/// 返回 `Ok(true)` = PASS，`Ok(false)` = FAIL，`Err` = 超时/连接失败/被停止。
fn run_calib(
    app: &AppHandle,
    shared: &Arc<PipelineShared>,
    stop: &Arc<AtomicBool>,
    core: &mut probe_rs::Core<'_>,
    mut rtt: Rtt,
    cfg: &PipelineConfig,
) -> Result<bool, String> {
    let mut parser = Parser::new();
    let deadline = SystemTime::now() + Duration::from_secs(CALIB_TIMEOUT_SECS);

    let send = |rtt: &mut Rtt, core: &mut probe_rs::Core<'_>, frame: &[u8]| -> Result<(), String> {
        let Some(down) = rtt.down_channel(0) else {
            return Err("未找到下行通道 0".to_string());
        };
        down.write(core, frame).map(|_| ()).map_err(|e| format!("下发命令失败: {e}"))
    };
    send(&mut rtt, core, &crate::proto::config(cfg.ch_mask & 0x0F, cfg.ref_da))?;
    send(&mut rtt, core, &crate::proto::start())?;

    let mut buf = [0u8; 1024];
    let mut pending: Vec<u8> = Vec::new();
    loop {
        if stop.load(Ordering::Relaxed) {
            return Err("已停止".to_string());
        }
        if SystemTime::now() > deadline {
            return Err(format!("校准超时（>{CALIB_TIMEOUT_SECS}s）"));
        }
        let channel = rtt
            .up_channel(0)
            .ok_or_else(|| "未找到上行通道 0".to_string())?;
        let n = channel.read(core, &mut buf).map_err(|e| format!("读上行失败: {e}"))?;
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
            let line = parse_line(&text);
            let msg = line.message.clone();
            append_line(shared, app, line);
            // feed 期望去掉 [LEVEL] file:line (func): 前缀后的 message；传整行 text 会永不匹配
            if parser.feed(&msg) {
                push_report(shared, app, &parser.report);
            }
        }
        if pending.len() > 4096 {
            pending.clear();
        }
        if let Some(pass) = calib_pass(&parser.report) {
            return Ok(pass);
        }
        if parser.report.phase == "fail" {
            return Ok(false);
        }
    }
}

/// 附着 APP 固件收集启动日志，直到校验日志通过或超时/条数上限。
fn run_verify_log(
    app: &AppHandle,
    shared: &Arc<PipelineShared>,
    stop: &Arc<AtomicBool>,
    core: &mut probe_rs::Core<'_>,
    mut rtt: Rtt,
) -> Result<(), String> {
    let mut buf = [0u8; 1024];
    let mut pending: Vec<u8> = Vec::new();
    let mut lines: Vec<String> = Vec::new();
    let deadline = SystemTime::now() + Duration::from_millis(VERIFY_LOG_TIMEOUT_MS);
    loop {
        if stop.load(Ordering::Relaxed) {
            return Err("已停止".to_string());
        }
        if SystemTime::now() > deadline {
            return Err(format!("启动日志超时（>{VERIFY_LOG_TIMEOUT_MS}ms）"));
        }
        let channel = rtt
            .up_channel(0)
            .ok_or_else(|| "未找到上行通道 0".to_string())?;
        let n = channel.read(core, &mut buf).map_err(|e| format!("读上行失败: {e}"))?;
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
            let text = String::from_utf8_lossy(&one).into_owned();
            let line = parse_line(&text);
            append_line(shared, app, line);
            lines.push(text);
        }
        if pending.len() > 4096 {
            pending.clear();
        }
        if lines.len() >= VERIFY_LOG_LINES || verify_log_pass(&lines) {
            break;
        }
    }
    if verify_log_pass(&lines) {
        Ok(())
    } else {
        Err("APP 启动验证失败：未同时出现 Device/App Version 与 Init success".to_string())
    }
}

struct ActiveGuard(Arc<PipelineShared>);

impl Drop for ActiveGuard {
    fn drop(&mut self) {
        self.0.status.lock().running = false;
    }
}

fn pipeline_loop(app: AppHandle, cfg: PipelineConfig, stop: Arc<AtomicBool>, shared: Arc<PipelineShared>) {
    let _active = ActiveGuard(shared.clone());
    shared.status.lock().running = true;
    loop {
        if stop.load(Ordering::Relaxed) {
            set_status(&shared, "idle", None, "已停止");
            emit_status(&app, &shared);
            return;
        }
        let mut session = match open_session(&cfg) {
            Ok(s) => s,
            Err(e) => {
                set_status(&shared, "flash_dut", Some(false), format!("打开会话失败: {e}"));
                emit_status(&app, &shared);
                return;
            }
        };

        // Step: FlashDut
        if stop.load(Ordering::Relaxed) {
            set_status(&shared, "idle", None, "已停止");
            emit_status(&app, &shared);
            return;
        }
        set_status(&shared, "flash_dut", None, format!("烧录校准固件（约数十秒，请稍候）{}", cfg.dut_path.display()));
        let _ = app.emit("prod://status", shared.status.lock().clone());
        if let Err(e) = flash::flash_file(&mut session, &cfg.dut_path) {
            set_status(&shared, "flash_dut", Some(false), e);
            emit_status(&app, &shared);
            return;
        }

        // Step: AttachCalib + RunCalib
        {
            let mut core = match session.core(0) {
                Ok(c) => c,
                Err(e) => {
                    set_status(&shared, "attach_calib", Some(false), format!("选择核心失败: {e}"));
                    emit_status(&app, &shared);
                    return;
                }
            };
            set_status(&shared, "attach_calib", None, "正在附着校准固件 RTT…");
            emit_status(&app, &shared);
            let rtt = match attach_rtt(&app, &shared, &stop, &mut core) {
                Ok(r) => r,
                Err(e) => {
                    set_status(&shared, "attach_calib", Some(false), e);
                    emit_status(&app, &shared);
                    return;
                }
            };
            set_status(&shared, "run_calib", None, "开始校准（67 点并行扫描 → 拟合 → 落盘 → 复检）…");
            emit_status(&app, &shared);
            match run_calib(&app, &shared, &stop, &mut core, rtt, &cfg) {
                Ok(true) => {
                    set_status(&shared, "run_calib", Some(true), "校准 PASS");
                }
                Ok(false) => {
                    set_status(&shared, "run_calib", Some(false), "校准 FAIL");
                    emit_status(&app, &shared);
                    return;
                }
                Err(e) => {
                    set_status(&shared, "run_calib", Some(false), e);
                    emit_status(&app, &shared);
                    return;
                }
            }
            emit_status(&app, &shared);
        }
        // core/rtt 已 drop，session 重新可变

        // Step: FlashApp
        if stop.load(Ordering::Relaxed) {
            set_status(&shared, "idle", None, "已停止");
            emit_status(&app, &shared);
            return;
        }
        set_status(&shared, "flash_app", None, format!("烧录应用固件（约数十秒，请稍候）{}", cfg.app_path.display()));
        let _ = app.emit("prod://status", shared.status.lock().clone());
        if let Err(e) = flash::flash_file(&mut session, &cfg.app_path) {
            set_status(&shared, "flash_app", Some(false), e);
            emit_status(&app, &shared);
            return;
        }

        // Step: AttachVerify
        {
            let mut core = match session.core(0) {
                Ok(c) => c,
                Err(e) => {
                    set_status(&shared, "attach_verify", Some(false), format!("选择核心失败: {e}"));
                    emit_status(&app, &shared);
                    return;
                }
            };
            set_status(&shared, "attach_verify", None, "正在附着 APP 固件 RTT…");
            emit_status(&app, &shared);
            let rtt = match attach_rtt(&app, &shared, &stop, &mut core) {
                Ok(r) => r,
                Err(e) => {
                    set_status(&shared, "attach_verify", Some(false), e);
                    emit_status(&app, &shared);
                    return;
                }
            };
            match run_verify_log(&app, &shared, &stop, &mut core, rtt) {
                Ok(()) => set_status(&shared, "done", Some(true), "启动验证通过，流程完成 PASS"),
                Err(e) => {
                    set_status(&shared, "done", Some(false), e);
                    emit_status(&app, &shared);
                    return;
                }
            }
        }
        emit_status(&app, &shared);

        if !cfg.auto_next {
            return;
        }
        // 连续多台：等待操作员换板后点「下一台」
        set_status(&shared, "wait_next", Some(true), "本台 PASS，换板后点『下一台』继续");
        emit_status(&app, &shared);
        {
            let mut gate = shared.next_gate.0.lock();
            loop {
                if *gate {
                    *gate = false;
                    break;
                }
                if stop.load(Ordering::Relaxed) {
                    return;
                }
                shared.next_gate.1.wait(&mut gate);
            }
        }
        set_status(&shared, "idle", None, "开始下一台");
        let _ = app.emit("prod://status", shared.status.lock().clone());
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn report(verify_pass: Option<bool>) -> CalibReport {
        let mut r = CalibReport::default();
        r.verify_pass = verify_pass;
        r
    }

    #[test]
    fn calib_pass_true() {
        assert_eq!(calib_pass(&report(Some(true))), Some(true));
    }

    #[test]
    fn calib_pass_false() {
        assert_eq!(calib_pass(&report(Some(false))), Some(false));
    }

    #[test]
    fn calib_pass_pending() {
        assert_eq!(calib_pass(&report(None)), None);
    }

    #[test]
    fn verify_log_pass_both_signals() {
        let lines = vec![
            "[INFO] Application.cpp:87 (VersionPrint): Device: AD_4CH, App Version: v1.9".to_string(),
            "[INFO] Application.cpp:54 (Init): Init success".to_string(),
        ];
        assert!(verify_log_pass(&lines));
    }

    #[test]
    fn verify_log_pass_missing_device() {
        let lines = vec!["[INFO] Application.cpp:54 (Init): Init success".to_string()];
        assert!(!verify_log_pass(&lines));
    }

    #[test]
    fn verify_log_pass_missing_init() {
        let lines = vec!["[INFO] Application.cpp:87 (VersionPrint): Device: AD_4CH, App Version: v1.9".to_string()];
        assert!(!verify_log_pass(&lines));
    }

    #[test]
    fn verify_log_pass_requires_init_after_device() {
        // 顺序无关，只要两条都出现过即通过
        let lines = vec![
            "[INFO] Application.cpp:54 (Init): Init success".to_string(),
            "[INFO] Application.cpp:87 (VersionPrint): Device: DA_4CH, App Version: v1.9".to_string(),
        ];
        assert!(verify_log_pass(&lines));
    }
}
