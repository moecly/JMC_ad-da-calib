import { useCallback, useEffect, useState } from "react";
import type { LogLine, PipelineStatus, ProbeInfo } from "../types";
import * as api from "../lib/ipc";

const MAX_LINES = 2000;

export function useBackend() {
  const [pipeline, setPipeline] = useState<PipelineStatus | null>(null);
  const [lines, setLines] = useState<LogLine[]>([]);
  const [probes, setProbes] = useState<ProbeInfo[]>([]);

  useEffect(() => {
    let disposed = false;
    const unlisteners: Array<() => void> = [];
    (async () => {
      const buffered = await api.getLog();
      if (disposed) return;
      setLines(buffered.slice(-MAX_LINES));
      const pipe = await api.getPipelineStatus();
      if (disposed) return;
      setPipeline(pipe);
      unlisteners.push(
        await api.onLine((line) => {
          setLines((prev) => {
            const next = prev.concat(line);
            return next.length > MAX_LINES ? next.slice(next.length - MAX_LINES) : next;
          });
        }),
        await api.onStatus((status) => setPipeline(status)),
        await api.onReport((report) => setPipeline((prev) => (prev ? { ...prev, report } : prev))),
      );
    })();
    return () => {
      disposed = true;
      unlisteners.forEach((u) => u());
    };
  }, []);

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
    await api.clearLog();
  }, []);

  return {
    pipeline,
    lines,
    probes,
    refreshProbes,
    clearAll,
  };
}
