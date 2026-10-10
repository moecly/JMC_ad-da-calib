import { invoke } from "@tauri-apps/api/core";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";
import type { CalibReport, LogLine, ProbeInfo, Snapshot, Status } from "../types";

export const listProbes = () => invoke<ProbeInfo[]>("list_probes");
export const connect = (probeIndex: number, chip: string) =>
  invoke<void>("connect", { probeIndex, chip });
export const disconnect = () => invoke<void>("disconnect");
export const getSnapshot = () => invoke<Snapshot>("get_snapshot");
export const clearReport = () => invoke<void>("clear_report");
export const loadLogFile = (path: string) => invoke<void>("load_log_file", { path });

export const calibConfig = (mask: number, refDa: number[]) =>
  invoke<void>("calib_config", { mask, refDa });
export const calibStart = () => invoke<void>("calib_start");
export const calibPause = () => invoke<void>("calib_pause");
export const calibResume = () => invoke<void>("calib_resume");
export const calibRestart = () => invoke<void>("calib_restart");
export const calibStop = () => invoke<void>("calib_stop");
export const manualSetDa = (ch: number, val: number, mode: number) =>
  invoke<void>("manual_set_da", { ch, val, mode });
export const manualReadAd = (mode: number) => invoke<void>("manual_read_ad", { mode });
export const calibStatus = () => invoke<void>("calib_status");
export const calibVerify = (mask: number, refDa: number[]) =>
  invoke<void>("calib_verify", { mask, refDa });
export const resetTarget = (runOnly = false) => invoke<void>("reset_target", { runOnly });
export const manualBegin = () => invoke<void>("manual_begin");
export const manualAbort = () => invoke<void>("manual_abort");
export const manualSetRaw = (ch: number, raw: number) => invoke<void>("manual_set_raw", { ch, raw });
export const manualAdd = (ch: number, ideal: number) => invoke<void>("manual_add", { ch, ideal });
export const manualFit = () => invoke<void>("manual_fit");
export const manualSave = () => invoke<void>("manual_save");

export const onLine = (cb: (line: LogLine) => void): Promise<UnlistenFn> =>
  listen<LogLine>("rtt://line", (e) => cb(e.payload));
export const onReport = (cb: (report: CalibReport) => void): Promise<UnlistenFn> =>
  listen<CalibReport>("calib://report", (e) => cb(e.payload));
export const onStatus = (cb: (status: Status) => void): Promise<UnlistenFn> =>
  listen<Status>("rtt://status", (e) => cb(e.payload));
