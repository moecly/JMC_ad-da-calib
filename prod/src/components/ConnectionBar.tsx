import { useState } from "react";
import type { ProbeInfo, Status } from "../types";
import * as api from "../lib/ipc";
import { IconLog } from "./icons";
import { Button, Input, Select, StatusDot } from "./ui";

interface Props {
  probes: ProbeInfo[];
  status: Status;
  probeName: string | null;
  onRefresh: () => Promise<ProbeInfo[]>;
  logOpen: boolean;
  onToggleLog: () => void;
  logCount: number;
}

const timeout = (ms: number) =>
  new Promise<never>((_, reject) => setTimeout(() => reject(new Error("操作超时")), ms));

export function ConnectionBar({
  probes,
  status,
  probeName,
  onRefresh,
  logOpen,
  onToggleLog,
  logCount,
}: Props) {
  const [probeIndex, setProbeIndex] = useState(0);
  const [chip, setChip] = useState("AT32F425K8T7");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const connected = status.connected;

  const toggle = async () => {
    setBusy(true);
    setError(null);
    try {
      if (connected) {
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
    <header className="flex flex-wrap items-center gap-2 border-b border-line bg-panel px-4 py-2">
      <StatusDot state={status.state} />
      <span className="text-[13px] font-medium">{status.message || (connected ? "已连接" : "未连接")}</span>
      {probeName && <span className="text-xs text-ink-faint">{probeName}</span>}

      <div className="ml-auto flex flex-wrap items-center gap-2">
        <Select
          value={probeIndex}
          disabled={connected || busy}
          onChange={(e) => setProbeIndex(Number(e.currentTarget.value))}
        >
          {probes.length === 0 && <option value={0}>无探针</option>}
          {probes.map((p) => (
            <option key={p.index} value={p.index}>
              {p.name}
              {p.serial ? ` (${p.serial})` : ""}
            </option>
          ))}
        </Select>
        <Button size="sm" variant={logOpen ? "primary" : "secondary"} onClick={onToggleLog}>
          <IconLog /> 日志 {logCount}
        </Button>
        <Button size="sm" disabled={busy} onClick={() => void onRefresh()}>
          刷新
        </Button>
        <Input
          className="w-40"
          value={chip}
          disabled={connected || busy}
          onChange={(e) => setChip(e.currentTarget.value)}
        />
        <Button
          variant="primary"
          size="sm"
          disabled={busy || (!connected && probes.length === 0)}
          onClick={() => void toggle()}
        >
          {connected ? "断开" : "连接"}
        </Button>
      </div>
      {status.hint && (
        <div className="w-full select-all rounded-lg bg-warn-soft px-3 py-2 font-mono text-xs text-warn-ink">
          {status.hint}
        </div>
      )}
      {error && <div className="w-full text-xs text-danger-ink">{error}</div>}
    </header>
  );
}
