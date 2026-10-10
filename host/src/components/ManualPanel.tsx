import { useState } from "react";
import type { CalibReport } from "../types";
import * as api from "../lib/ipc";
import { Button, Card, CardTitle, Field, Pill } from "./ui";

const clamp = (v: number) => Math.max(0, Math.min(65535, v));

const boardLabel = (dir: string | null) => (dir === "ad" ? "AD 采集板" : dir === "da" ? "DA 输出板" : "?");

const inputCls =
  "w-24 rounded border border-slate-600 bg-slate-800 px-2 py-1 font-mono text-slate-200 disabled:opacity-50";

export function ManualPanel({ connected, report }: { connected: boolean; report: CalibReport | null }) {
  const [ch, setCh] = useState(0);
  const [vfs, setVfs] = useState("10.000");
  const [mode, setMode] = useState<"volt" | "code">("volt");
  const [value, setValue] = useState("0");
  const [raw, setRaw] = useState("32768");
  const [error, setError] = useState<string | null>(null);

  const run = (fn: () => Promise<void>) => {
    setError(null);
    fn().catch((e) => setError(String(e)));
  };

  const manual = report?.manual;
  const boardDir = report?.board_dir ?? null;
  const inManual = manual?.on ?? false;
  const channel = manual?.channels[ch];
  const fitted = channel?.fitted ?? false;
  const ready = connected && inManual;

  if (!boardDir) {
    return (
      <Card>
        <CardTitle>手动校准</CardTitle>
        <div className="text-xs text-slate-500">未识别板型方向：连接目标后点「概览 → 刷新信息」。</div>
      </Card>
    );
  }

  const vfsNum = Number(vfs) || 10;
  const ideal =
    mode === "code"
      ? clamp(Math.round(Number(value) || 0))
      : clamp(Math.round(((Number(value) || 0) / vfsNum) * 65535));

  return (
    <div className="flex flex-col gap-4">
      <Card>
        <CardTitle
          right={
            manual?.verify_pass !== undefined && manual?.verify_pass !== null ? (
              <Pill label={manual.verify_pass ? "复检 PASS" : "复检 FAIL"} tone={manual.verify_pass ? "green" : "red"} />
            ) : undefined
          }
        >
          手动校准 · {boardLabel(boardDir)}
        </CardTitle>

        <div className="flex flex-wrap items-center gap-3">
          <Button variant="primary" disabled={!connected || inManual} onClick={() => run(api.manualBegin)}>
            开始手动校准
          </Button>
          <Button variant="danger" disabled={!connected || !inManual} onClick={() => run(api.manualAbort)}>
            放弃
          </Button>
          <Pill label={inManual ? "手动模式" : "未进入"} tone={inManual ? "blue" : "slate"} />
          <Pill label={`落盘 ${manual?.saved ? "已保存" : "未保存"}`} tone={manual?.saved ? "green" : "slate"} />
        </div>

        <div className="mt-4 flex flex-wrap items-center gap-4">
          <Field label="满量程 VFS" suffix="V">
            <input className={inputCls} value={vfs} disabled={!ready} onChange={(e) => setVfs(e.currentTarget.value)} />
          </Field>
          <Field label="录入口径">
            <select
              className="rounded border border-slate-600 bg-slate-800 px-2 py-1 text-slate-200 disabled:opacity-50"
              value={mode}
              disabled={!ready}
              onChange={(e) => setMode(e.currentTarget.value as "volt" | "code")}
            >
              <option value="volt">电压 V</option>
              <option value="code">理想码 Code</option>
            </select>
          </Field>
          <Field label="通道">
            <select
              className="rounded border border-slate-600 bg-slate-800 px-2 py-1 text-slate-200 disabled:opacity-50"
              value={ch}
              disabled={!ready}
              onChange={(e) => setCh(Number(e.currentTarget.value))}
            >
              {[0, 1, 2, 3].map((c) => (
                <option key={c} value={c}>
                  ch{c}（{manual?.channels[c]?.points.length ?? 0} 点）
                </option>
              ))}
            </select>
          </Field>
        </div>

        {boardDir === "ad" ? (
          <div className="mt-4 flex flex-col gap-3">
            <div className="flex flex-wrap items-center gap-3">
              <Button disabled={!connected} onClick={() => run(api.manualReadAd)}>
                读当前码
              </Button>
              <span className="font-mono text-xs text-slate-300">
                {(report?.ad_read ?? [null, null, null, null]).map((v, i) => `ch${i}:${v ?? "—"}`).join("  ")}
              </span>
            </div>
            <div className="flex flex-wrap items-center gap-3">
              <Field label={mode === "volt" ? "施加电压" : "施加码"} suffix={mode === "volt" ? "V" : ""}>
                <input
                  className={inputCls}
                  value={value}
                  disabled={!ready}
                  onChange={(e) => setValue(e.currentTarget.value)}
                />
              </Field>
              <span className="font-mono text-xs text-slate-500">ideal={ideal}</span>
              <Button disabled={!ready} onClick={() => run(() => api.manualAdd(ch, ideal))}>
                采集点
              </Button>
            </div>
          </div>
        ) : (
          <div className="mt-4 flex flex-col gap-3">
            <div className="flex flex-wrap items-center gap-3">
              <Field label="raw 码">
                <input
                  className={inputCls}
                  value={raw}
                  disabled={!ready}
                  onChange={(e) => setRaw(e.currentTarget.value)}
                />
              </Field>
              <Button disabled={!ready} onClick={() => run(() => api.manualSetRaw(ch, clamp(Number(raw) || 0)))}>
                写入
              </Button>
            </div>
            <div className="flex flex-wrap items-center gap-3">
              <Field label={mode === "volt" ? "实测电压" : "实测码"} suffix={mode === "volt" ? "V" : ""}>
                <input
                  className={inputCls}
                  value={value}
                  disabled={!ready}
                  onChange={(e) => setValue(e.currentTarget.value)}
                />
              </Field>
              <span className="font-mono text-xs text-slate-500">ideal={ideal}</span>
              <Button disabled={!ready} onClick={() => run(() => api.manualAdd(ch, ideal))}>
                采集点
              </Button>
            </div>
          </div>
        )}

        <div className="mt-4 flex flex-wrap items-center gap-3">
          <Button disabled={!ready} onClick={() => run(api.manualFit)}>
            拟合
          </Button>
          <Button variant="primary" disabled={!ready || !fitted} onClick={() => run(api.manualSave)}>
            写入 Flash
          </Button>
          {channel?.resid && (
            <span className="font-mono text-xs text-slate-400">
              残差 max={channel.resid.max.toFixed(2)} rmse={channel.resid.rmse.toFixed(2)}
            </span>
          )}
        </div>

        {error && <div className="mt-3 text-xs text-red-400">{error}</div>}
      </Card>

      {manual && manual.errors.length > 0 && (
        <Card>
          <CardTitle>手动校准错误 ({manual.errors.length})</CardTitle>
          <ul className="max-h-40 space-y-0.5 overflow-auto font-mono text-xs text-red-300">
            {manual.errors.map((e, i) => (
              <li key={i}>{e}</li>
            ))}
          </ul>
        </Card>
      )}

      <Card>
        <CardTitle>采样点 · ch{ch}</CardTitle>
        {(channel?.points.length ?? 0) === 0 ? (
          <div className="text-xs text-slate-500">暂无采样点</div>
        ) : (
          <table className="w-full border-collapse text-xs">
            <thead>
              <tr className="text-left text-slate-400">
                <th className="border-b border-slate-700 px-2 py-1">n</th>
                <th className="border-b border-slate-700 px-2 py-1">ideal</th>
                <th className="border-b border-slate-700 px-2 py-1">raw</th>
              </tr>
            </thead>
            <tbody>
              {channel?.points.map((p) => (
                <tr key={p.n} className="font-mono text-slate-200">
                  <td className="border-b border-slate-800 px-2 py-1">{p.n}</td>
                  <td className="border-b border-slate-800 px-2 py-1">{p.ideal}</td>
                  <td className="border-b border-slate-800 px-2 py-1">{p.raw}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </Card>

      {manual && (manual.channels.some((c) => c.resid) || manual.channels.some((c) => c.verify)) && (
        <Card>
          <CardTitle>拟合 / 复检</CardTitle>
          <table className="w-full border-collapse text-xs">
            <thead>
              <tr className="text-left text-slate-400">
                <th className="border-b border-slate-700 px-2 py-1">通道</th>
                <th className="border-b border-slate-700 px-2 py-1">拟合</th>
                <th className="border-b border-slate-700 px-2 py-1">残差 max/rmse</th>
                <th className="border-b border-slate-700 px-2 py-1">复检 max/rmse</th>
                <th className="border-b border-slate-700 px-2 py-1">系数 c0..c3</th>
              </tr>
            </thead>
            <tbody>
              {manual.channels.map((c) => (
                <tr key={c.ch} className="text-slate-200">
                  <td className="border-b border-slate-800 px-2 py-1 font-mono">ch{c.ch}</td>
                  <td className="border-b border-slate-800 px-2 py-1">
                    <Pill label={c.fitted ? "已拟合" : "未拟合"} tone={c.fitted ? "green" : "slate"} />
                  </td>
                  <td className="border-b border-slate-800 px-2 py-1 font-mono">
                    {c.resid ? `${c.resid.max.toFixed(2)} / ${c.resid.rmse.toFixed(2)}` : "—"}
                  </td>
                  <td className="border-b border-slate-800 px-2 py-1 font-mono">
                    {c.verify ? `${c.verify.max.toFixed(2)} / ${c.verify.rmse.toFixed(2)}` : "—"}
                  </td>
                  <td className="border-b border-slate-800 px-2 py-1 font-mono text-slate-400">
                    {c.coeff ? c.coeff.map((v) => v.toFixed(6)).join(", ") : "—"}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </Card>
      )}
    </div>
  );
}
