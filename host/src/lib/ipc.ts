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

export const onLine = (cb: (line: LogLine) => void): Promise<UnlistenFn> =>
  listen<LogLine>("rtt://line", (e) => cb(e.payload));
export const onReport = (cb: (report: CalibReport) => void): Promise<UnlistenFn> =>
  listen<CalibReport>("calib://report", (e) => cb(e.payload));
export const onStatus = (cb: (status: Status) => void): Promise<UnlistenFn> =>
  listen<Status>("rtt://status", (e) => cb(e.payload));
