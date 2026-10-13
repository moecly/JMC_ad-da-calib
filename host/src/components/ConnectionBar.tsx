import { useState } from "react";
import type { ProbeInfo, Status } from "../types";
import * as api from "../lib/ipc";
import { Button, Input, Select, StatusDot } from "./ui";

interface Props {
  probes: ProbeInfo[];
  status: Status;
  probeName: string | null;
  onRefresh: () => Promise<ProbeInfo[]>;
}

const timeout = (ms: number) =>
  new Promise<never>((_, reject) => setTimeout(() => reject(new Error("操作超时")), ms));

export function ConnectionBar({ probes, status, probeName, onRefresh }: Props) {
  const [probeIndex, setProbeIndex] = useState(0);
  const [chip, setChip] = useState("AT32F425K8T7");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const connected = status.connected;
  // 工作线程存续期间（连接中/搜索中）后端 conn 已被占用：此时若仍显示「连接」，点击只会撞上
  // 后端的「已连接」错误，而且没有任何办法中止搜索；按钮必须反映「有会话」这一事实
  const session = connected || status.state === "connecting" || status.state === "searching";
  const showTargetCtl = status.state !== "disconnected" || connected;

  const toggle = async () => {
    setBusy(true);
    setError(null);
    try {
      if (session) {
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
    <header className="flex flex-wrap items-center gap-2 border-b border-line bg-panel px-4 py-2">
      <StatusDot state={status.state} />
      <span className="text-[13px] font-medium">{status.message || (connected ? "已连接" : "未连接")}</span>
      {probeName && <span className="text-xs text-ink-faint">{probeName}</span>}

      <div className="ml-auto flex flex-wrap items-center gap-2">
        {showTargetCtl && (
          <>
            <Button
              size="sm"
              disabled={!connected}
              onClick={() => void api.resetTarget(false).catch((e) => setError(String(e)))}
            >
              复位并运行
            </Button>
            <Button
              size="sm"
              disabled={!connected}
              onClick={() => void api.resetTarget(true).catch((e) => setError(String(e)))}
            >
              仅运行
            </Button>
          </>
        )}
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
          disabled={busy || (!session && probes.length === 0)}
          onClick={() => void toggle()}
        >
          {session ? "断开" : "连接"}
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
