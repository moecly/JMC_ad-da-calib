mod calib;
mod rtt;

use std::collections::VecDeque;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use parking_lot::Mutex;
use serde::Serialize;
use tauri::{AppHandle, Emitter, State};

use calib::{CalibReport, Parser};
use rtt::{LogLine, ProbeInfo, Status};

#[derive(Default)]
pub struct AppState {
    conn: Mutex<Option<(std::thread::JoinHandle<()>, Arc<AtomicBool>)>>,
    probe_name: Mutex<Option<String>>,
    status: Arc<Mutex<Status>>,
    lines: Arc<Mutex<VecDeque<LogLine>>>,
    parser: Arc<Mutex<Parser>>,
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
    let mut conn = state.conn.lock();
    if let Some((handle, _)) = conn.as_ref() {
        if !handle.is_finished() {
            return Err("已连接".to_string());
        }
    }
    if let Some((handle, _)) = conn.take() {
        let _ = handle.join();
    }
    let name = rtt::list_probes()
        .ok()
        .and_then(|v| v.into_iter().nth(probe_index))
        .map(|p| p.name);
    let Some(name) = name else {
        return Err(format!("未找到探针 #{probe_index}"));
    };
    let stop = Arc::new(AtomicBool::new(false));
    let handle = rtt::spawn_worker(
        app.clone(),
        stop.clone(),
        probe_index,
        chip.clone(),
        state.lines.clone(),
        state.parser.clone(),
        state.status.clone(),
    );
    *conn = Some((handle, stop));
    *state.probe_name.lock() = Some(name);
    rtt::emit_status(&app, &state.status, false, format!("正在连接 {chip}…"));
    Ok(())
}

#[tauri::command]
fn disconnect(app: AppHandle, state: State<AppState>) -> Result<(), String> {
    let mut conn = state.conn.lock();
    if let Some((handle, stop)) = conn.take() {
        stop.store(true, Ordering::Relaxed);
        let _ = handle.join();
    }
    drop(conn);
    *state.probe_name.lock() = None;
    rtt::emit_status(&app, &state.status, false, "已断开");
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
            load_log_file
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
