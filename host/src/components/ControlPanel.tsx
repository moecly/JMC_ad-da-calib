import { useState } from "react";
import type { CalibReport } from "../types";
import type { CalibParamsCtl } from "../hooks/useCalibParams";
import * as api from "../lib/ipc";
import { Button, Card, CardTitle, Input, Pill, Segmented, Select } from "./ui";
import { ChannelLinkMatrix } from "./ChannelLinkMatrix";

interface Props {
  connected: boolean;
  report: CalibReport | null;
  ctl: CalibParamsCtl;
}

export function ControlPanel({ connected, report, ctl }: Props) {
  const [enabled, setEnabled] = useState<boolean[]>([false, false, false, false]);
  const [daSel, setDaSel] = useState<number[]>([0, 0, 0, 0]);
  const [manualCh, setManualCh] = useState(0);
  const [manualIdeal, setManualIdeal] = useState(25000);
  const [manualMode, setManualMode] = useState(0);
  const [error, setError] = useState<string | null>(null);

  const run = (fn: () => Promise<void>) => {
    setError(null);
    fn().catch((e) => setError(String(e)));
  };

  const applyConfig = () => {
    let mask = 0;
    enabled.forEach((on, ch) => {
      if (on) mask |= 1 << ch;
    });
    run(async () => {
      await ctl.apply();
      await api.calibConfig(mask, daSel);
    });
  };

  const maskOf = () => enabled.reduce((m, on, ch) => (on ? m | (1 << ch) : m), 0);

  // 启动/重启先下发参数与当前选择，避免未点「配置」时沿用设备默认 mask=0x0F
  const applyThen = (cmd: () => Promise<void>) => {
    const mask = maskOf();
    if (mask === 0) return;
    run(async () => {
      await ctl.apply();
      await api.calibConfig(mask, daSel);
      await cmd();
    });
  };

  const channels = report?.channels ?? [];
  const adRead = report?.ad_read ?? [null, null, null, null];
  const dutIsDa = report?.board_dir === "da";
  const dutName = dutIsDa ? "DA" : "AD";
  const refName = dutIsDa ? "AD" : "DA";

  return (
    <Card className="flex flex-col gap-4">
      <CardTitle>自动校准</CardTitle>

      <ChannelLinkMatrix
        enabled={enabled}
        daSel={daSel}
        dutName={dutName}
        refName={refName}
        disabled={!connected}
        onToggle={(ch, on) => setEnabled((p) => p.map((x, i) => (i === ch ? on : x)))}
        onDaSelect={(ch, da) => setDaSel((p) => p.map((x, i) => (i === ch ? da : x)))}
      />

      <div className="flex flex-wrap items-center gap-2">
        <Button size="sm" disabled={!connected} onClick={applyConfig}>
          配置
        </Button>
        <Button size="sm" variant="primary" disabled={!connected || maskOf() === 0} onClick={() => applyThen(api.calibStart)}>
          启动
        </Button>
        <Button size="sm" disabled={!connected} onClick={() => run(api.calibPause)}>
          暂停
        </Button>
        <Button size="sm" disabled={!connected} onClick={() => run(api.calibResume)}>
          继续
        </Button>
        <Button size="sm" variant="primary" disabled={!connected || maskOf() === 0} onClick={() => applyThen(api.calibRestart)}>
          重启
        </Button>
        <Button size="sm" disabled={!connected} onClick={() => run(api.calibStop)}>
          停止
        </Button>
        <Button size="sm" disabled={!connected} onClick={() => run(api.calibStatus)}>
          刷新状态
        </Button>
      </div>

      <div className="flex flex-wrap items-center gap-2">
        <span className="text-xs text-ink-soft">手动</span>
        <Select value={manualCh} disabled={!connected} onChange={(e) => setManualCh(Number(e.currentTarget.value))}>
          {[0, 1, 2, 3].map((m) => (
            <option key={m} value={m}>
              {dutName} ch{m}
            </option>
          ))}
        </Select>
        <Segmented
          options={[
            { value: 0, label: "raw" },
            { value: 1, label: "校准后" },
          ]}
          value={manualMode}
          onChange={setManualMode}
        />
        <Input
          type="number"
          min={0}
          max={65535}
          className="w-24"
          value={manualIdeal}
          disabled={!connected}
          onChange={(e) => setManualIdeal(Number(e.currentTarget.value))}
        />
        <Button
          size="sm"
          disabled={!connected}
          onClick={() => run(() => api.manualSetDa(manualCh, manualIdeal, manualMode))}
        >
          {manualMode === 1 ? (dutIsDa ? "写本机 DA ideal" : "写 REF DA ideal") : dutIsDa ? "写本机 DA raw" : "写 REF DA raw"}
        </Button>
        <Button size="sm" disabled={!connected} onClick={() => run(() => api.manualReadAd(manualMode))}>
          {dutIsDa ? (manualMode === 1 ? "读 REF AD ideal" : "读 REF AD raw") : "读 AD"}
        </Button>
        <span className="font-mono tabular-nums text-ink-soft">
          {adRead.map((v, i) => `ch${i}:${v == null ? "—" : dutIsDa && manualMode === 1 ? v.toFixed(1) : String(v)}`).join("  ")}
        </span>
      </div>

      <div className="flex flex-wrap items-center gap-3 text-xs">
        <span className="text-ink-soft">
          状态：<span className="font-mono text-ink">{report?.phase ?? "—"}</span>
        </span>
        <span className="text-ink-soft">
          当前 run：<span className="font-mono text-ink">{report?.run_ch ?? "—"}</span>
        </span>
        <span className="text-ink-soft">
          复检：<span className="font-mono text-ink">{ctl.params.skip_verify ? "跳过" : "开启"}</span>
        </span>
        <span className="flex items-center gap-2">
          {channels.map((ch) => (
            <Pill key={ch.ch} label={`ch${ch.ch} ${ch.valid ? "已校准" : "未校准"}`} tone={ch.valid ? "green" : "slate"} />
          ))}
        </span>
      </div>

      {error && <div className="text-xs text-danger-ink">{error}</div>}
    </Card>
  );
}
