import { useState } from "react";
import type { ProbeInfo, Status } from "../types";
import * as api from "../lib/ipc";
import { Button } from "./ui";

interface Props {
  probes: ProbeInfo[];
  status: Status;
  probeName: string | null;
  onRefresh: () => Promise<ProbeInfo[]>;
}

const dotClass = (state: string) => {
  switch (state) {
    case "attached":
      return "bg-green-500";
    case "searching":
      return "bg-amber-400 animate-pulse";
    case "error":
      return "bg-red-500";
    case "connecting":
      return "bg-blue-400 animate-pulse";
    default:
      return "bg-slate-500";
  }
};

const timeout = (ms: number) =>
  new Promise<never>((_, reject) => setTimeout(() => reject(new Error("操作超时")), ms));

export function ConnectionBar({ probes, status, probeName, onRefresh }: Props) {
  const [probeIndex, setProbeIndex] = useState(0);
  const [chip, setChip] = useState("AT32F425K8T7");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const connected = status.connected;
  const showTargetCtl = status.state !== "disconnected" || connected;

  const toggle = async () => {
    setBusy(true);
    setError(null);
    try {
      if (connected) {
        // Rust 侧 disconnect 有界返回；此处再兜底一层，避免按钮永久 busy
        await Promise.race([api.disconnect(), timeout(5000)]);
      } else {
        await api.connect(probeIndex, chip);
      }
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="sticky top-0 z-10 border-b border-slate-700 bg-slate-900/95 px-4 py-2 text-sm backdrop-blur">
      <div className="flex items-center gap-3">
        <span className="font-semibold text-slate-200">AD/DA 校准上位机</span>
        <div className={`h-3 w-3 rounded-full ${dotClass(status.state)}`} />
        <span className="text-slate-300">{status.message || (connected ? "已连接" : "未连接")}</span>
        {probeName && <span className="text-slate-500">[{probeName}]</span>}

        <div className="ml-auto flex items-center gap-2">
          {showTargetCtl && (
            <>
              <Button disabled={!connected} onClick={() => void api.resetTarget(false).catch((e) => setError(String(e)))}>
                复位并运行
              </Button>
              <Button disabled={!connected} onClick={() => void api.resetTarget(true).catch((e) => setError(String(e)))}>
                仅运行
              </Button>
            </>
          )}
          <select
            className="rounded border border-slate-600 bg-slate-800 px-2 py-1 text-slate-200 disabled:opacity-50"
            value={probeIndex}
            disabled={connected || busy}
            onChange={(e) => setProbeIndex(Number(e.currentTarget.value))}
          >
            {probes.length === 0 && (
              <option value={0} className="bg-slate-800 text-slate-100">
                无探针
              </option>
            )}
            {probes.map((p) => (
              <option key={p.index} value={p.index} className="bg-slate-800 text-slate-100">
                {p.name}
                {p.serial ? ` (${p.serial})` : ""}
              </option>
            ))}
          </select>
          <Button disabled={busy} onClick={() => void onRefresh()}>
            刷新
          </Button>
          <input
            className="w-44 rounded border border-slate-600 bg-slate-800 px-2 py-1 text-slate-200 disabled:opacity-50"
            value={chip}
            disabled={connected || busy}
            onChange={(e) => setChip(e.currentTarget.value)}
          />
          <Button
            variant="primary"
            disabled={busy || (!connected && probes.length === 0)}
            onClick={() => void toggle()}
          >
            {connected ? "断开" : "连接"}
          </Button>
        </div>
      </div>
      {status.hint && (
        <div className="mt-1 select-all font-mono text-xs text-amber-300">{status.hint}</div>
      )}
      {error && <div className="mt-1 text-xs text-red-400">{error}</div>}
    </div>
  );
}
