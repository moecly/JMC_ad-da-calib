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
    let lastState = "";
    (async () => {
      const snapshot = await api.getSnapshot();
      if (disposed) return;
      setStatus(snapshot.status);
      lastState = snapshot.status.state;
      setReport(snapshot.report);
      setProbeName(snapshot.probe_name);
      setLines(snapshot.lines.slice(-MAX_LINES));
      unlisteners.push(
        await api.onLine((line) => appendLines([line])),
        await api.onReport((report) => setReport(report)),
        await api.onStatus((status) => {
          setStatus(status);
          if (status.state === "disconnected") setProbeName(null);
          // 附着成功时拉一次设备/板型/REF 信息，供概览卡展示
          if (status.connected && status.state === "attached" && lastState !== "attached") {
            void api.calibStatus().catch(() => {});
          }
          lastState = status.state;
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
