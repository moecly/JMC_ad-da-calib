use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread::sleep;
use std::time::{Duration, SystemTime};

use parking_lot::Mutex;
use probe_rs::probe::list::Lister;
use probe_rs::rtt::{Rtt, ScanRegion};
use probe_rs::Permissions;
use serde::{Deserialize, Serialize};
use tauri::{AppHandle, Emitter};

use crate::calib::{CalibReport, Parser};
use crate::flash;
use crate::proto::{self, CalibParams};
use crate::rtt::{hint_for, parse_line, LogLine};

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
    pub params: CalibParams,
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
}

/// APP 启动校验是否通过：固件上电打印 `Device: <板型>, App Version: vX.Y`、`Init success`，
/// 以及本板标定记录加载标记（见 `calib_load_ok`）。三个信号都必须出现。
pub fn verify_log_pass(lines: &[String], board_dir: Option<&str>) -> bool {
    let has_device =
        lines.iter().any(|l| l.contains("Device:") && l.contains("App Version:"));
    let has_init = lines.iter().any(|l| l.contains("Init success"));
    has_device && has_init && calib_load_ok(lines, board_dir)
}

/// 标定记录加载成功标记：APP 在 `BoardInit()` 主动触发一次换算，打印
/// `calib ad load ok mask=15` / `calib da load ok mask=15`（失败为 `calib <board> load fail (raw passthrough)`）。
/// 烧 APP 走 keep_unwritten_bytes（保护 `0x0800F000`），该标记证明记录仍完好且被 APP 读出；
/// mask 必须非 0——加载成功但无有效通道等于没应用任何系数，不能放行。
pub fn calib_load_ok(lines: &[String], board_dir: Option<&str>) -> bool {
    let markers: &[&str] = match board_dir {
        Some("ad") => &["calib ad load ok mask="],
        Some("da") => &["calib da load ok mask="],
        // 校准阶段未报板型时放宽为任一板型标记
        _ => &["calib ad load ok mask=", "calib da load ok mask="],
    };
    lines.iter().any(|l| {
        markers.iter().any(|m| match l.find(m) {
            Some(i) => mask_value(&l[i + m.len()..]) != 0,
            None => false,
        })
    })
}

/// 兼容十进制/十六进制两种打印形式：只看是否非 0，任一表示法下判据一致。
fn mask_value(rest: &str) -> u32 {
    let digits: String = rest.chars().take_while(|c| c.is_ascii_hexdigit()).collect();
    u32::from_str_radix(&digits, 16).unwrap_or(0)
}

fn verify_missing(lines: &[String], board_dir: Option<&str>) -> String {
    let mut missing: Vec<&str> = Vec::new();
    if !lines.iter().any(|l| l.contains("Device:") && l.contains("App Version:")) {
        missing.push("Device/App Version");
    }
    if !lines.iter().any(|l| l.contains("Init success")) {
        missing.push("Init success");
    }
    if !calib_load_ok(lines, board_dir) {
        missing.push(match board_dir {
            Some("ad") => "calib ad load ok mask≠0",
            Some("da") => "calib da load ok mask≠0",
            _ => "calib ad|da load ok mask≠0",
        });
    }
    missing.join(" / ")
}

/// 校准是否通过：复检被跳过（以拟合残差门限放行）时以落盘结论为准，
/// 否则以固件复检结论 `calib: verify PASS/FAIL` 为准（解析器写入 `verify_pass`）。
pub fn calib_pass(report: &CalibReport) -> Option<bool> {
    if report.verify_skipped {
        return Some(report.saved);
    }
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
                    return Err(format!("未找到 RTT 控制块: {e}；{}", hint_for(&e)));
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
    send(&mut rtt, core, &proto::params(&cfg.params))?;
    send(&mut rtt, core, &proto::config(cfg.ch_mask & 0x0F, cfg.ref_da))?;
    send(&mut rtt, core, &proto::start())?;

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
    board_dir: Option<&str>,
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
        if lines.len() >= VERIFY_LOG_LINES || verify_log_pass(&lines, board_dir) {
            break;
        }
    }
    if verify_log_pass(&lines, board_dir) {
        Ok(())
    } else {
        Err(format!("APP 启动验证失败：缺少 {}", verify_missing(&lines, board_dir)))
    }
}

struct ActiveGuard {
    app: AppHandle,
    shared: Arc<PipelineShared>,
}

impl Drop for ActiveGuard {
    fn drop(&mut self) {
        self.shared.status.lock().running = false;
        // 线程退出即广播：只清 running 不发事件，前端会一直停在「运行中」，按钮态不恢复
        emit_status(&self.app, &self.shared);
    }
}

fn pipeline_loop(app: AppHandle, cfg: PipelineConfig, stop: Arc<AtomicBool>, shared: Arc<PipelineShared>) {
    let _active = ActiveGuard {
        app: app.clone(),
        shared: shared.clone(),
    };
    shared.status.lock().running = true;
    {
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
        emit_status(&app, &shared);
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
        emit_status(&app, &shared);
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
            let board_dir = shared.status.lock().report.board_dir.clone();
            match run_verify_log(&app, &shared, &stop, &mut core, rtt, board_dir.as_deref()) {
                Ok(()) => set_status(&shared, "done", Some(true), "启动验证通过，流程完成 PASS"),
                Err(e) => {
                    set_status(&shared, "done", Some(false), e);
                    emit_status(&app, &shared);
                    return;
                }
            }
        }
        emit_status(&app, &shared);

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

    fn report_skipped(saved: bool) -> CalibReport {
        let mut r = CalibReport::default();
        r.verify_skipped = true;
        r.saved = saved;
        r
    }

    #[test]
    fn calib_pass_true() {
        assert_eq!(calib_pass(&report(Some(true))), Some(true));
    }

    #[test]
    fn calib_pass_skipped_saved() {
        assert_eq!(calib_pass(&report_skipped(true)), Some(true));
    }

    #[test]
    fn calib_pass_skipped_not_saved() {
        assert_eq!(calib_pass(&report_skipped(false)), Some(false));
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
            "[INFO] Board.cpp:24 (BoardInit): calib ad load ok mask=15".to_string(),
            "[INFO] Application.cpp:54 (Init): Init success".to_string(),
        ];
        assert!(verify_log_pass(&lines, Some("ad")));
    }

    #[test]
    fn verify_log_pass_missing_device() {
        let lines = vec![
            "[INFO] Board.cpp:24 (BoardInit): calib ad load ok mask=15".to_string(),
            "[INFO] Application.cpp:54 (Init): Init success".to_string(),
        ];
        assert!(!verify_log_pass(&lines, Some("ad")));
    }

    #[test]
    fn verify_log_pass_missing_init() {
        let lines = vec![
            "[INFO] Application.cpp:87 (VersionPrint): Device: AD_4CH, App Version: v1.9".to_string(),
            "[INFO] Board.cpp:24 (BoardInit): calib ad load ok mask=15".to_string(),
        ];
        assert!(!verify_log_pass(&lines, Some("ad")));
    }

    #[test]
    fn verify_log_pass_requires_init_after_device() {
        // 顺序无关，只要三条都出现过即通过
        let lines = vec![
            "[INFO] Application.cpp:54 (Init): Init success".to_string(),
            "[INFO] Application.cpp:87 (VersionPrint): Device: DA_4CH, App Version: v1.9".to_string(),
            "[INFO] Board.cpp:19 (BoardInit): calib da load ok mask=15".to_string(),
        ];
        assert!(verify_log_pass(&lines, Some("da")));
    }

    #[test]
    fn verify_log_pass_requires_calib_load_ok() {
        // 缺标定记录加载标记：APP 未应用系数（或烧录后记录被破坏）
        let lines = vec![
            "[INFO] Application.cpp:87 (VersionPrint): Device: AD_4CH, App Version: v1.9".to_string(),
            "[INFO] Application.cpp:54 (Init): Init success".to_string(),
        ];
        assert!(!verify_log_pass(&lines, Some("ad")));
        assert_eq!(verify_missing(&lines, Some("ad")), "calib ad load ok mask≠0");
    }

    #[test]
    fn verify_log_pass_rejects_zero_mask() {
        // 记录 CRC 通过但无有效通道：等于没应用系数，不能放行
        let lines = vec![
            "[INFO] Application.cpp:87 (VersionPrint): Device: AD_4CH, App Version: v1.9".to_string(),
            "[INFO] Board.cpp:55 (BoardInit): calib ad load ok mask=0".to_string(),
            "[INFO] Application.cpp:54 (Init): Init success".to_string(),
        ];
        assert!(!verify_log_pass(&lines, Some("ad")));
    }

    #[test]
    fn verify_log_pass_rejects_wrong_board_marker() {
        // 板型匹配：AD 板只认 `calib ad load ok`，加载失败标记不算通过
        let da_marker = vec![
            "[INFO] Application.cpp:87 (VersionPrint): Device: AD_4CH, App Version: v1.9".to_string(),
            "[INFO] Board.cpp:46 (BoardInit): calib da load ok mask=15".to_string(),
            "[INFO] Application.cpp:54 (Init): Init success".to_string(),
        ];
        assert!(!verify_log_pass(&da_marker, Some("ad")));
        let fail_marker = vec![
            "[INFO] Application.cpp:87 (VersionPrint): Device: AD_4CH, App Version: v1.9".to_string(),
            "[INFO] Board.cpp:46 (BoardInit): calib ad load fail (raw passthrough)0".to_string(),
            "[INFO] Application.cpp:54 (Init): Init success".to_string(),
        ];
        assert!(!verify_log_pass(&fail_marker, Some("ad")));
    }

    #[test]
    fn verify_log_pass_accepts_any_board_marker_when_unknown() {
        let lines = vec![
            "[INFO] Application.cpp:87 (VersionPrint): Device: DA_4CH, App Version: v1.9".to_string(),
            "[INFO] Board.cpp:44 (BoardInit): calib da load ok mask=15".to_string(),
            "[INFO] Application.cpp:54 (Init): Init success".to_string(),
        ];
        assert!(verify_log_pass(&lines, None));
    }
}
