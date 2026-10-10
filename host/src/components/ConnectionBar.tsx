import { useState } from "react";
import type { ProbeInfo } from "../types";
import * as api from "../lib/ipc";

interface Props {
  probes: ProbeInfo[];
  connected: boolean;
  statusMessage: string;
  probeName: string | null;
  onRefresh: () => Promise<ProbeInfo[]>;
}

export function ConnectionBar({ probes, connected, statusMessage, probeName, onRefresh }: Props) {
  const [probeIndex, setProbeIndex] = useState(0);
  const [chip, setChip] = useState("AT32F425K8T7");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const toggle = async () => {
    setBusy(true);
    setError(null);
    try {
      if (connected) {
        await api.disconnect();
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
    <div className="flex items-center gap-3 border-b border-slate-700 bg-slate-900 px-4 py-2 text-sm">
      <span className="font-semibold text-slate-200">AD/DA 校准上位机</span>
      <div className={connected ? "h-3 w-3 rounded-full bg-green-500" : "h-3 w-3 rounded-full bg-slate-500"} />
      <span className="text-slate-300">{statusMessage || (connected ? "已连接" : "未连接")}</span>
      {probeName && <span className="text-slate-500">[{probeName}]</span>}

      <div className="ml-auto flex items-center gap-2">
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
        <button
          className="rounded border border-slate-600 px-2 py-1 text-slate-200 disabled:opacity-50"
          disabled={busy}
          onClick={() => void onRefresh()}
        >
          刷新
        </button>
        <input
          className="w-44 rounded border border-slate-600 bg-slate-800 px-2 py-1 text-slate-200 disabled:opacity-50"
          value={chip}
          disabled={connected || busy}
          onChange={(e) => setChip(e.currentTarget.value)}
        />
        <button
          className="rounded bg-blue-600 px-3 py-1 font-medium text-white disabled:cursor-not-allowed disabled:opacity-50"
          disabled={busy || (!connected && probes.length === 0)}
          onClick={() => void toggle()}
        >
          {connected ? "断开" : "连接"}
        </button>
      </div>
      {error && <span className="text-red-400">{error}</span>}
    </div>
  );
}
