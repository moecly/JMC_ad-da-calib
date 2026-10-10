mod calib;
mod proto;
mod rtt;

use std::collections::VecDeque;
use std::sync::atomic::{AtomicBool, AtomicU8, Ordering};
use std::sync::Arc;
use std::time::Duration;

use parking_lot::Mutex;
use serde::Serialize;
use tauri::{AppHandle, Emitter, State};

use calib::{CalibReport, Parser};
use rtt::{LogLine, ProbeInfo, Status};

#[derive(Default)]
pub struct AppState {
    conn: Mutex<Option<(std::thread::JoinHandle<()>, Arc<AtomicBool>)>>,
    control: Arc<AtomicU8>,
    probe_name: Mutex<Option<String>>,
    status: Arc<Mutex<Status>>,
    lines: Arc<Mutex<VecDeque<LogLine>>>,
    parser: Arc<Mutex<Parser>>,
    commands: Arc<Mutex<VecDeque<Vec<u8>>>>,
}

#[derive(Serialize)]
struct Snapshot {
    status: Status,
    probe_name: Option<String>,
    lines: Vec<LogLine>,
    report: CalibReport,
}

#[tauri::command]
fn list_probes() -> Result<Vec<ProbeInfo>, String> {
    rtt::list_probes()
}

#[tauri::command]
fn connect(
    app: AppHandle,
    state: State<AppState>,
    probe_index: usize,
    chip: String,
) -> Result<(), String> {
    {
        let conn = state.conn.lock();
        if let Some((handle, _)) = conn.as_ref() {
            if !handle.is_finished() {
                return Err("已连接".to_string());
            }
        }
    }
    // 取出旧会话，有界等待其退出；探针卡死时旧 handle 丢弃、线程自行结束
    if let Some((handle, _)) = state.conn.lock().take() {
        if handle.is_finished() {
            let _ = handle.join();
        } else {
            let (tx, rx) = std::sync::mpsc::channel::<()>();
            std::thread::spawn(move || {
                let _ = handle.join();
                let _ = tx.send(());
            });
            if rx.recv_timeout(Duration::from_secs(1)).is_err() {
                return Err("探针仍被上一个会话占用（可能已卡死）：拔插探针后重试".to_string());
            }
        }
    }
    let name = rtt::list_probes()
        .ok()
        .and_then(|v| v.into_iter().nth(probe_index))
        .map(|p| p.name);
    let Some(name) = name else {
        return Err(format!("未找到探针 #{probe_index}"));
    };
    let stop = Arc::new(AtomicBool::new(false));
    state.commands.lock().clear();
    let handle = rtt::spawn_worker(
        app.clone(),
        stop.clone(),
        state.control.clone(),
        probe_index,
        chip.clone(),
        state.lines.clone(),
        state.parser.clone(),
        state.status.clone(),
        state.commands.clone(),
    );
    *state.conn.lock() = Some((handle, stop));
    *state.probe_name.lock() = Some(name);
    rtt::emit_status(
        &app,
        &state.status,
        Status {
            connected: false,
            state: "connecting".to_string(),
            message: format!("正在连接 {chip}…"),
            hint: None,
            attempts: 0,
        },
    );
    Ok(())
}

#[tauri::command]
fn disconnect(app: AppHandle, state: State<AppState>) -> Result<(), String> {
    // conn.lock().take() 是临时 guard，join/recv_timeout 全程不持有 conn 锁，
    // 否则探针卡死时会冻结所有控制按钮
    let taken = state.conn.lock().take();
    let Some((handle, stop)) = taken else {
        rtt::emit_status(
            &app,
            &state.status,
            Status {
                connected: false,
                state: "disconnected".to_string(),
                message: "已断开".to_string(),
                hint: None,
                attempts: 0,
            },
        );
        return Ok(());
    };
    stop.store(true, Ordering::Relaxed);
    let message = if handle.is_finished() {
        let _ = handle.join();
        "已断开".to_string()
    } else {
        let (tx, rx) = std::sync::mpsc::channel::<()>();
        std::thread::spawn(move || {
            let _ = handle.join();
            let _ = tx.send(());
        });
        if rx.recv_timeout(Duration::from_secs(3)).is_ok() {
            "已断开".to_string()
        } else {
            "已断开（探针线程仍在释放，探针可能卡死：拔插探针后重连）".to_string()
        }
    };
    state.commands.lock().clear();
    *state.probe_name.lock() = None;
    rtt::emit_status(
        &app,
        &state.status,
        Status {
            connected: false,
            state: "disconnected".to_string(),
            message,
            hint: None,
            attempts: 0,
        },
    );
    Ok(())
}

#[tauri::command]
fn get_snapshot(state: State<AppState>) -> Snapshot {
    Snapshot {
        status: state.status.lock().clone(),
        probe_name: state.probe_name.lock().clone(),
        lines: state.lines.lock().iter().cloned().collect(),
        report: state.parser.lock().report.clone(),
    }
}

#[tauri::command]
fn clear_report(app: AppHandle, state: State<AppState>) {
    let report = {
        let mut parser = state.parser.lock();
        *parser = Parser::new();
        parser.report.clone()
    };
    let _ = app.emit("calib://report", report);
}

#[tauri::command]
async fn load_log_file(
    app: AppHandle,
    state: State<'_, AppState>,
    path: String,
) -> Result<(), String> {
    let content = std::fs::read_to_string(&path).map_err(|e| e.to_string())?;
    for raw in content.lines() {
        rtt::process_line(&app, &state.lines, &state.parser, raw);
    }
    Ok(())
}

fn push_command(state: &State<AppState>, frame: Vec<u8>) -> Result<(), String> {
    let connected = state
        .conn
        .lock()
        .as_ref()
        .map(|(h, _)| !h.is_finished())
        .unwrap_or(false);
    if !connected {
        return Err("未连接".to_string());
    }
    state.commands.lock().push_back(frame);
    Ok(())
}

#[tauri::command]
fn calib_config(state: State<AppState>, mask: u8, ref_da: Vec<u8>) -> Result<(), String> {
    if ref_da.len() != 4 {
        return Err("ref_da 需要 4 个元素".to_string());
    }
    let da = [ref_da[0] & 0x03, ref_da[1] & 0x03, ref_da[2] & 0x03, ref_da[3] & 0x03];
    push_command(&state, proto::config(mask & 0x0F, da))
}

#[tauri::command]
fn calib_start(state: State<AppState>) -> Result<(), String> {
    push_command(&state, proto::start())
}

#[tauri::command]
fn calib_pause(state: State<AppState>) -> Result<(), String> {
    push_command(&state, proto::pause())
}

#[tauri::command]
fn calib_resume(state: State<AppState>) -> Result<(), String> {
    push_command(&state, proto::resume())
}

#[tauri::command]
fn calib_restart(state: State<AppState>) -> Result<(), String> {
    push_command(&state, proto::restart())
}

#[tauri::command]
fn calib_stop(state: State<AppState>) -> Result<(), String> {
    push_command(&state, proto::stop())
}

#[tauri::command]
fn manual_set_da(state: State<AppState>, ch: u8, val: u16, mode: u8) -> Result<(), String> {
    push_command(&state, proto::set_da(ch & 0x03, val, mode))
}

#[tauri::command]
fn manual_read_ad(state: State<AppState>, mode: u8) -> Result<(), String> {
    push_command(&state, proto::read_ad(mode))
}

#[tauri::command]
fn calib_status(state: State<AppState>) -> Result<(), String> {
    push_command(&state, proto::status())
}

#[tauri::command]
fn calib_verify(state: State<AppState>, mask: u8, ref_da: Vec<u8>) -> Result<(), String> {
    if ref_da.len() != 4 {
        return Err("ref_da 需要 4 个元素".to_string());
    }
    let da = [ref_da[0] & 0x03, ref_da[1] & 0x03, ref_da[2] & 0x03, ref_da[3] & 0x03];
    push_command(&state, proto::config(mask & 0x0F, da))?;
    push_command(&state, proto::verify())
}

#[tauri::command]
fn reset_target(_app: AppHandle, state: State<AppState>, run_only: Option<bool>) -> Result<(), String> {
    let connected = state
        .conn
        .lock()
        .as_ref()
        .map(|(h, _)| !h.is_finished())
        .unwrap_or(false);
    if !connected {
        return Err("未连接".to_string());
    }
    state
        .control
        .store(if run_only.unwrap_or(false) { 2 } else { 1 }, Ordering::SeqCst);
    Ok(())
}

#[tauri::command]
fn manual_begin(state: State<AppState>) -> Result<(), String> {
    push_command(&state, proto::manual_begin())
}

#[tauri::command]
fn manual_abort(state: State<AppState>) -> Result<(), String> {
    push_command(&state, proto::manual_abort())
}

#[tauri::command]
fn manual_set_raw(state: State<AppState>, ch: u8, raw: u16) -> Result<(), String> {
    push_command(&state, proto::manual_set_raw(ch, raw))
}

#[tauri::command]
fn manual_add(state: State<AppState>, ch: u8, ideal: u16) -> Result<(), String> {
    push_command(&state, proto::manual_add(ch, ideal))
}

#[tauri::command]
fn manual_fit(state: State<AppState>) -> Result<(), String> {
    push_command(&state, proto::manual_fit())
}

#[tauri::command]
fn manual_save(state: State<AppState>) -> Result<(), String> {
    push_command(&state, proto::manual_save())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(AppState::default())
        .invoke_handler(tauri::generate_handler![
            list_probes,
            connect,
            disconnect,
            get_snapshot,
            clear_report,
            load_log_file,
            calib_config,
            calib_start,
            calib_pause,
            calib_resume,
            calib_restart,
            calib_stop,
            manual_set_da,
            manual_read_ad,
            calib_status,
            calib_verify,
            reset_target,
            manual_begin,
            manual_abort,
            manual_set_raw,
            manual_add,
            manual_fit,
            manual_save
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
