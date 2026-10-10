import { useState } from "react";
import type { CalibReport } from "../types";
import * as api from "../lib/ipc";

interface Props {
  connected: boolean;
  report: CalibReport | null;
}

const btn =
  "rounded border border-slate-600 px-2 py-1 text-slate-200 disabled:cursor-not-allowed disabled:opacity-40";

export function ControlPanel({ connected, report }: Props) {
  const [enabled, setEnabled] = useState<boolean[]>([false, false, false, false]);
  const [daSel, setDaSel] = useState<number[]>([0, 0, 0, 0]);
  const [manualCh, setManualCh] = useState(0);
  const [manualIdeal, setManualIdeal] = useState(25000);
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
    run(() => api.calibConfig(mask, daSel));
  };

  const maskOf = () => enabled.reduce((m, on, ch) => (on ? m | (1 << ch) : m), 0);

  // 启动/重启先下发当前选择，避免未点「配置」时沿用设备默认 mask=0x0F
  const applyThen = (cmd: () => Promise<void>) => {
    const mask = maskOf();
    if (mask === 0) return;
    run(async () => {
      await api.calibConfig(mask, daSel);
      await cmd();
    });
  };

  const channels = report?.channels ?? [];
  const adRead = report?.ad_read ?? [null, null, null, null];

  return (
    <div className="flex flex-col gap-3 border-b border-slate-800 p-4 text-sm">
      <div className="font-semibold text-slate-200">控制面板</div>

      <div className="flex flex-col gap-1 text-xs">
        <div className="text-slate-400">按连线选择单通道校准（AD chN ← REF DA chM）</div>
        {[0, 1, 2, 3].map((ch) => (
          <div key={ch} className="flex items-center gap-3">
            <label className="flex w-24 items-center gap-1 text-slate-200">
              <input
                type="checkbox"
                checked={enabled[ch]}
                disabled={!connected}
                onChange={(e) => {
                  const v = e.currentTarget.checked;
                  setEnabled((p) => p.map((x, i) => (i === ch ? v : x)));
                }}
              />
              AD ch{ch}
            </label>
            <select
              className="rounded border border-slate-600 bg-slate-800 px-2 py-0.5 text-slate-200 disabled:opacity-50"
              value={daSel[ch]}
              disabled={!connected}
              onChange={(e) => {
                const v = Number(e.currentTarget.value);
                setDaSel((p) => p.map((x, i) => (i === ch ? v : x)));
              }}
            >
              {[0, 1, 2, 3].map((m) => (
                <option key={m} value={m}>
                  DA ch{m}
                </option>
              ))}
            </select>
          </div>
        ))}
      </div>

      <div className="flex flex-wrap gap-2 text-xs">
        <button className={btn} disabled={!connected} onClick={applyConfig}>
          配置
        </button>
        <button className={btn} disabled={!connected || maskOf() === 0} onClick={() => applyThen(api.calibStart)}>
          启动
        </button>
        <button className={btn} disabled={!connected} onClick={() => run(api.calibPause)}>
          暂停
        </button>
        <button className={btn} disabled={!connected} onClick={() => run(api.calibResume)}>
          继续
        </button>
        <button className={btn} disabled={!connected || maskOf() === 0} onClick={() => applyThen(api.calibRestart)}>
          重启
        </button>
        <button className={btn} disabled={!connected} onClick={() => run(api.calibStop)}>
          停止
        </button>
        <button className={btn} disabled={!connected} onClick={() => run(api.calibStatus)}>
          刷新状态
        </button>
      </div>

      <div className="flex flex-wrap items-center gap-2 text-xs">
        <span className="text-slate-400">手动</span>
        <select
          className="rounded border border-slate-600 bg-slate-800 px-2 py-1 text-slate-200 disabled:opacity-50"
          value={manualCh}
          disabled={!connected}
          onChange={(e) => setManualCh(Number(e.currentTarget.value))}
        >
          {[0, 1, 2, 3].map((m) => (
            <option key={m} value={m}>
              DA ch{m}
            </option>
          ))}
        </select>
        <input
          type="number"
          min={0}
          max={65535}
          className="w-24 rounded border border-slate-600 bg-slate-800 px-2 py-1 text-slate-200 disabled:opacity-50"
          value={manualIdeal}
          disabled={!connected}
          onChange={(e) => setManualIdeal(Number(e.currentTarget.value))}
        />
        <button
          className={btn}
          disabled={!connected}
          onClick={() => run(() => api.manualSetDa(manualCh, manualIdeal))}
        >
          写码
        </button>
        <button className={btn} disabled={!connected} onClick={() => run(api.manualReadAd)}>
          读 AD
        </button>
        <span className="font-mono text-slate-300">
          {adRead.map((v, i) => `ch${i}:${v ?? "—"}`).join("  ")}
        </span>
      </div>

      <div className="flex flex-wrap items-center gap-3 text-xs">
        <span className="text-slate-400">
          状态：<span className="font-mono text-slate-200">{report?.phase ?? "—"}</span>
        </span>
        <span className="text-slate-400">
          当前 run：<span className="font-mono text-slate-200">{report?.run_ch ?? "—"}</span>
        </span>
        <span className="flex items-center gap-2">
          {channels.map((ch) => (
            <span
              key={ch.ch}
              className={`rounded border px-1.5 py-0.5 ${
                ch.valid
                  ? "border-green-600 bg-green-600/20 text-green-400"
                  : "border-slate-600 bg-slate-700/20 text-slate-400"
              }`}
            >
              ch{ch.ch} {ch.valid ? "已校准" : "未校准"}
            </span>
          ))}
        </span>
      </div>

      {error && <div className="text-xs text-red-400">{error}</div>}
    </div>
  );
}
