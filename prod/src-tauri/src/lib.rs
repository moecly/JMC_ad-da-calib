mod calib;
mod flash;
mod pipeline;
mod proto;
mod rtt;

use std::collections::VecDeque;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

use parking_lot::Mutex;
use serde::Serialize;
use tauri::{AppHandle, Emitter, State};

use pipeline::{PipelineConfig, PipelineShared, PipelineStatus};
use rtt::{LogLine, ProbeInfo, Status};

#[derive(Default)]
pub struct AppState {
    pipeline: Mutex<Option<(std::thread::JoinHandle<()>, Arc<AtomicBool>)>>,
    shared: Arc<PipelineShared>,
    conn_status: Arc<Mutex<Status>>,
    probe_name: Mutex<Option<String>>,
    lines: Arc<Mutex<VecDeque<LogLine>>>,
}

#[derive(Serialize)]
struct Snapshot {
    status: Status,
    probe_name: Option<String>,
    lines: Vec<LogLine>,
    report: calib::CalibReport,
}

#[tauri::command]
fn list_probes() -> Result<Vec<ProbeInfo>, String> {
    rtt::list_probes()
}

/// 生产流程线程独占探针，这里「连接」只记录选择的探针（不做 RTT 附着），
/// 真正占用探针发生在 start_pipeline 打开会话时。
#[tauri::command]
fn connect(app: AppHandle, state: State<AppState>, probe_index: usize, chip: String) -> Result<(), String> {
    let name = rtt::list_probes()
        .ok()
        .and_then(|v| v.into_iter().nth(probe_index))
        .map(|p| p.name)
        .ok_or_else(|| format!("未找到探针 #{probe_index}"))?;
    *state.probe_name.lock() = Some(name.clone());
    let status = Status {
        connected: true,
        state: "attached".to_string(),
        message: format!("已选择探针 {name}（{chip}）"),
        hint: None,
        attempts: 0,
    };
    rtt::emit_status(&app, &state.conn_status, status);
    Ok(())
}

#[tauri::command]
fn disconnect(app: AppHandle, state: State<AppState>) -> Result<(), String> {
    // 停止并回收流程线程，释放探针设备锁；否则流程结束后探针仍被占，只能重启应用重连
    {
        let taken = state.pipeline.lock().take();
        if let Some((handle, stop)) = taken {
            stop.store(true, Ordering::Relaxed);
            if handle.is_finished() {
                let _ = handle.join();
            } else {
                let (tx, rx) = std::sync::mpsc::channel::<()>();
                std::thread::spawn(move || {
                    let _ = handle.join();
                    let _ = tx.send(());
                });
                if rx.recv_timeout(Duration::from_secs(3)).is_err() {
                    return Err("流程线程仍在释放（可能已卡死）：拔插探针后重试".to_string());
                }
            }
        }
    }
    *state.probe_name.lock() = None;
    rtt::emit_status(
        &app,
        &state.conn_status,
        Status {
            connected: false,
            state: "disconnected".to_string(),
            message: "已断开".to_string(),
            hint: None,
            attempts: 0,
        },
    );
    Ok(())
}

#[tauri::command]
fn get_snapshot(state: State<AppState>) -> Snapshot {
    Snapshot {
        status: state.conn_status.lock().clone(),
        probe_name: state.probe_name.lock().clone(),
        lines: state.lines.lock().iter().cloned().collect(),
        report: state.shared.status.lock().report.clone(),
    }
}

#[tauri::command]
fn start_pipeline(
    app: AppHandle,
    state: State<AppState>,
    cfg: PipelineConfig,
) -> Result<(), String> {
    {
        let conn = state.pipeline.lock();
        if let Some((handle, _)) = conn.as_ref() {
            if !handle.is_finished() {
                return Err("流程已在运行".to_string());
            }
        }
    }
    // 清理上一个流程线程；有界等待其退出
    if let Some((handle, _)) = state.pipeline.lock().take() {
        if handle.is_finished() {
            let _ = handle.join();
        } else {
            let (tx, rx) = std::sync::mpsc::channel::<()>();
            std::thread::spawn(move || {
                let _ = handle.join();
                let _ = tx.send(());
            });
            if rx.recv_timeout(Duration::from_secs(3)).is_err() {
                return Err("上一个流程线程仍占用探针（可能已卡死）：拔插探针后重试".to_string());
            }
        }
    }
    let stop = Arc::new(AtomicBool::new(false));
    let handle = pipeline::spawn_pipeline(app.clone(), cfg, stop.clone(), state.shared.clone());
    *state.pipeline.lock() = Some((handle, stop));
    Ok(())
}

#[tauri::command]
fn stop_pipeline(app: AppHandle, state: State<AppState>) -> Result<(), String> {
    let taken = state.pipeline.lock().take();
    let Some((handle, stop)) = taken else {
        return Ok(());
    };
    stop.store(true, Ordering::Relaxed);
    if handle.is_finished() {
        let _ = handle.join();
    } else {
        let (tx, rx) = std::sync::mpsc::channel::<()>();
        std::thread::spawn(move || {
            let _ = handle.join();
            let _ = tx.send(());
        });
        if rx.recv_timeout(Duration::from_secs(3)).is_err() {
            return Err("流程线程仍在释放（可能已卡死）：拔插探针后重试".to_string());
        }
    }
    let _ = app.emit("prod://status", state.shared.status.lock().clone());
    Ok(())
}

#[tauri::command]
fn get_pipeline_status(state: State<AppState>) -> PipelineStatus {
    state.shared.status.lock().clone()
}

/// 「下一台」：唤醒连续模式中停在 `wait_next` 的线程，继续下一台流水。
#[tauri::command]
fn next_pipeline(state: State<AppState>) -> Result<(), String> {
    let shared = state.shared.clone();
    let status = shared.status.lock().clone();
    if status.step != "wait_next" {
        return Err("当前不在『等待下一台』状态".to_string());
    }
    {
        let mut gate = shared.next_gate.0.lock();
        *gate = true;
    }
    shared.next_gate.1.notify_one();
    Ok(())
}

#[tauri::command]
fn clear_log(app: AppHandle, state: State<AppState>) {
    let mut status = state.shared.status.lock();
    status.log_lines.clear();
    status.report = calib::CalibReport::default();
    let clone = status.clone();
    let _ = app.emit("prod://status", clone);
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
            start_pipeline,
            stop_pipeline,
            next_pipeline,
            get_pipeline_status,
            clear_log
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
