import { useCallback, useEffect, useState } from "react";
import type { CalibReport, LogLine, ProbeInfo, Status } from "../types";
import * as api from "../lib/ipc";

const MAX_LINES = 2000;

export function useBackend() {
  const [status, setStatus] = useState<Status>({
    connected: false,
    state: "disconnected",
    message: "",
    hint: null,
    attempts: 0,
  });
  const [report, setReport] = useState<CalibReport | null>(null);
  const [lines, setLines] = useState<LogLine[]>([]);
  const [probes, setProbes] = useState<ProbeInfo[]>([]);
  const [probeName, setProbeName] = useState<string | null>(null);

  const appendLines = useCallback((incoming: LogLine[]) => {
    setLines((prev) => {
      const next = prev.concat(incoming);
      return next.length > MAX_LINES ? next.slice(next.length - MAX_LINES) : next;
    });
  }, []);

  useEffect(() => {
    let disposed = false;
    const unlisteners: Array<() => void> = [];
    (async () => {
      const snapshot = await api.getSnapshot();
      if (disposed) return;
      setStatus(snapshot.status);
      setReport(snapshot.report);
      setProbeName(snapshot.probe_name);
      setLines(snapshot.lines.slice(-MAX_LINES));
      unlisteners.push(
        await api.onLine((line) => appendLines([line])),
        await api.onReport((report) => setReport(report)),
        await api.onStatus((status) => {
          setStatus(status);
          if (status.state === "disconnected") setProbeName(null);
          // 每次附着（含复位后重建 RTT、手动会话被清）都拉一次设备/角色/会话状态
          if (status.connected && status.state === "attached") {
            void api.calibStatus().catch(() => {});
          }
        }),
      );
    })();
    return () => {
      disposed = true;
      unlisteners.forEach((u) => u());
    };
  }, [appendLines]);

  const refreshProbes = useCallback(async () => {
    try {
      const list = await api.listProbes();
      setProbes(list);
      return list;
    } catch {
      setProbes([]);
      return [];
    }
  }, []);

  const clearAll = useCallback(async () => {
    setLines([]);
    await api.clearReport();
  }, []);

  return {
    status,
    report,
    lines,
    probes,
    probeName,
    refreshProbes,
    clearAll,
    api,
  };
}
