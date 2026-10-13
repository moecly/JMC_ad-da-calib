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
use probe_rs::probe::list::Lister;
use probe_rs::rtt::{Rtt, ScanRegion};
use probe_rs::{CoreStatus, Permissions};
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

/// 「连接」= 探针自检：打开探针、附着芯片、尝试读 RTT 首行以确认固件在跑，随后释放探针。
/// 用 spawn_blocking 执行：Tauri 同步 command 跑在主线程，probe 附着 + 读 RTT 可能耗时，会冻结 UI。
#[tauri::command]
async fn connect(app: AppHandle, state: State<'_, AppState>, probe_index: usize, chip: String) -> Result<(), String> {
    let (name, message) = tauri::async_runtime::spawn_blocking(move || probe_selfcheck(probe_index, &chip))
        .await
        .map_err(|e| format!("连接任务失败: {e}"))??;
    *state.probe_name.lock() = Some(name);
    let status = Status {
        connected: true,
        state: "attached".to_string(),
        message,
        hint: None,
        attempts: 0,
    };
    rtt::emit_status(&app, &state.conn_status, status);
    Ok(())
}

fn probe_selfcheck(probe_index: usize, chip: &str) -> Result<(String, String), String> {
    let info = Lister::new()
        .list_all()
        .into_iter()
        .nth(probe_index)
        .ok_or_else(|| format!("未找到探针 #{probe_index}"))?;
    let name = info.identifier.clone();
    let probe = info.open().map_err(|e| format!("打开探针失败: {e}"))?;
    let mut session = probe
        .attach(chip, Permissions::default())
        .map_err(|e| format!("附着芯片 {chip} 失败: {e}"))?;
    let mut core = session.core(0).map_err(|e| format!("选择核心失败: {e}"))?;
    // 目标可能停在 halt 态（上一次烧录后），固件不跑就没有 RTT 控制块
    if !matches!(core.status(), Ok(CoreStatus::Running)) {
        let _ = core.run();
    }
    let rtt = Rtt::attach(&mut core)
        .or_else(|_| Rtt::attach_region(&mut core, &ScanRegion::range(0x2000_0000..0x2000_5000)));
    let message = match rtt {
        Ok(mut r) => {
            let mut buf = [0u8; 256];
            let mut first = String::new();
            if let Some(ch) = r.up_channel(0) {
                for _ in 0..50 {
                    if let Ok(n) = ch.read(&mut core, &mut buf) {
                        if n > 0 {
                            first = String::from_utf8_lossy(&buf[..n])
                                .lines()
                                .next()
                                .unwrap_or("")
                                .trim()
                                .to_string();
                            break;
                        }
                    }
                    std::thread::sleep(Duration::from_millis(20));
                }
            }
            if first.is_empty() {
                format!("已连接 {name} · RTT 就绪（暂无日志）")
            } else {
                format!("已连接 {name} · {first}")
            }
        }
        Err(_) => format!("已连接 {name}（未找到 RTT，固件可能未运行）"),
    };
    // session/core/rtt 在此作用域结束 drop，探针立即释放，不阻塞后续流程
    Ok((name, message))
}

#[tauri::command]
async fn disconnect(app: AppHandle, state: State<'_, AppState>) -> Result<(), String> {
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
            connect,
            disconnect,
            get_snapshot,
            start_pipeline,
            stop_pipeline,
            get_pipeline_status,
            clear_log
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
