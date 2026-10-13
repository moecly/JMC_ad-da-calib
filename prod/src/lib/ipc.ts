import { invoke } from "@tauri-apps/api/core";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";
import type { CalibReport, LogLine, PipelineConfig, PipelineStatus, ProbeInfo, Snapshot, Status } from "../types";

export const listProbes = () => invoke<ProbeInfo[]>("list_probes");
export const connect = (probeIndex: number, chip: string) =>
  invoke<void>("connect", { probeIndex, chip });
export const disconnect = () => invoke<void>("disconnect");
export const getSnapshot = () => invoke<Snapshot>("get_snapshot");

export const startPipeline = (cfg: PipelineConfig) =>
  invoke<void>("start_pipeline", { cfg });
export const stopPipeline = () => invoke<void>("stop_pipeline");
export const nextPipeline = () => invoke<void>("next_pipeline");
export const getPipelineStatus = () => invoke<PipelineStatus>("get_pipeline_status");
export const clearLog = () => invoke<void>("clear_log");

export const onLine = (cb: (line: LogLine) => void): Promise<UnlistenFn> =>
  listen<LogLine>("prod://line", (e) => cb(e.payload));
export const onReport = (cb: (report: CalibReport) => void): Promise<UnlistenFn> =>
  listen<CalibReport>("prod://report", (e) => cb(e.payload));
export const onStatus = (cb: (status: PipelineStatus) => void): Promise<UnlistenFn> =>
  listen<PipelineStatus>("prod://status", (e) => cb(e.payload));
export const onConnStatus = (cb: (status: Status) => void): Promise<UnlistenFn> =>
  listen<Status>("rtt://status", (e) => cb(e.payload));
