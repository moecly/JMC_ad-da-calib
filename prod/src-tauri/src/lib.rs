mod calib;
mod flash;
mod pipeline;
mod proto;
mod rtt;

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

use parking_lot::Mutex;
use tauri::{AppHandle, Emitter, State};

use pipeline::{PipelineConfig, PipelineShared, PipelineStatus};
use rtt::{LogLine, ProbeInfo};

#[derive(Default)]
pub struct AppState {
    pipeline: Mutex<Option<(std::thread::JoinHandle<()>, Arc<AtomicBool>)>>,
    shared: Arc<PipelineShared>,
}

#[tauri::command]
fn list_probes() -> Result<Vec<ProbeInfo>, String> {
    rtt::list_probes()
}

/// 返回已缓冲日志：UI 重载（HMR/刷新）后用已有行回填日志抽屉。
#[tauri::command]
fn get_log(state: State<AppState>) -> Vec<LogLine> {
    state.shared.status.lock().log_lines.clone()
}

#[tauri::command]
async fn start_pipeline(
    app: AppHandle,
    state: State<'_, AppState>,
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
    // 清理上一个流程线程；有界等待其退出。先 take 再 join，避免 join 期间持有 pipeline 锁。
    let prev = state.pipeline.lock().take();
    if let Some((handle, _)) = prev {
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
async fn stop_pipeline(app: AppHandle, state: State<'_, AppState>) -> Result<(), String> {
    // 只置停止标志并立即返回：非烧录阶段线程会在下一检查点退出；
    // 烧录阶段是 probe-rs 黑盒调用无法中途打断，需等本次烧录结束。
    // 不在此 join，避免 UI 在烧录期间卡死；线程回收由下一次 start_pipeline 的 join 兜底。
    {
        let conn = state.pipeline.lock();
        let Some((_, stop)) = conn.as_ref() else {
            return Ok(());
        };
        stop.store(true, Ordering::Relaxed);
    }
    state.shared.status.lock().message = "正在停止…（烧录阶段需等本次烧录结束）".to_string();
    pipeline::emit_status(&app, &state.shared);
    Ok(())
}

#[tauri::command]
fn get_pipeline_status(state: State<AppState>) -> PipelineStatus {
    state.shared.status.lock().clone()
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
            get_log,
            start_pipeline,
            stop_pipeline,
            get_pipeline_status,
            clear_log
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
