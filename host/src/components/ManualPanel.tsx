import { useState } from "react";
import type { CalibReport } from "../types";
import type { CalibParamsCtl } from "../hooks/useCalibParams";
import * as api from "../lib/ipc";
import { ManualChart } from "./ManualChart";
import { Button, Card, CardTitle, Field, Input, Pill, Select } from "./ui";

const clamp = (v: number) => Math.max(0, Math.min(65535, v));

const MANUAL_MIN_POINTS = 8;

const boardLabel = (dir: string | null) => (dir === "ad" ? "AD 采集板" : dir === "da" ? "DA 输出板" : "?");

export function ManualPanel({ connected, report, ctl }: { connected: boolean; report: CalibReport | null; ctl: CalibParamsCtl }) {
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
  const replaced = manual?.last_replace && manual.last_replace.ch === ch ? manual.last_replace : null;
  const replacedPoint = replaced ? (channel?.points ?? []).find((p) => p.n === replaced.n) ?? null : null;
  const points = channel?.points.length ?? 0;
  const pendingRaw = channel?.pending_raw ?? null;
  const replaceAt = pendingRaw !== null ? (channel?.points.find((p) => p.raw === pendingRaw) ?? null) : null;
  const samplingCh = manual?.sampling_ch ?? null;
  const anyPoints = (manual?.channels ?? []).some((c) => c.points.length > 0);
  const fittedChs = (manual?.channels ?? []).filter((c) => c.fitted).map((c) => c.ch);
  const isManualFw = report?.role === "MANUAL";
  const ready = connected && isManualFw && inManual;

  if (!boardDir) {
    return (
      <Card>
        <CardTitle>手动校准</CardTitle>
        <div className="text-xs text-ink-faint">未识别板型方向：连接目标后点「概览 → 刷新信息」。</div>
      </Card>
    );
  }

  const vfsNum = Number(vfs) || 10;
  const ideal =
    mode === "code"
      ? clamp(Math.round(Number(value) || 0))
      : clamp(Math.round(((Number(value) || 0) / vfsNum) * 65535));
  const adReplaceAt = channel?.points.find((p) => p.ideal === ideal) ?? null;

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

        {connected && !isManualFw && (
          <div className="mb-3 rounded-lg bg-warn-soft px-3 py-2 text-xs text-warn-ink">
            {report?.role
              ? `当前固件角色为 ${report.role}，不含手动校准命令。请烧录 CALIB_ROLE=MANUAL 固件（如 ${boardDir === "da" ? "DA" : "AD"}_4CH MANUAL）后再操作。`
              : "未获取到固件角色：连接后在「概览 → 刷新信息」。"}
          </div>
        )}

        <div className="flex flex-wrap items-center gap-3">
          <Button
            variant="primary"
            size="sm"
            disabled={!connected || !isManualFw || inManual}
            onClick={() =>
              run(async () => {
                await ctl.apply();
                await api.manualBegin();
              })
            }
          >
            开始手动校准
          </Button>
          <Button
            variant="danger"
            size="sm"
            disabled={!connected || !isManualFw || !inManual}
            onClick={() => run(api.manualAbort)}
          >
            放弃
          </Button>
          <Pill label={inManual ? "手动模式" : "未进入"} tone={inManual ? "blue" : "slate"} />
          <Pill label={`落盘 ${manual?.saved ? "已保存" : "未保存"}`} tone={manual?.saved ? "green" : "slate"} />
        </div>

        <div className="mt-4 flex flex-wrap items-center gap-4">
          <Field label="满量程 VFS" suffix="V">
            <Input className="w-24" value={vfs} disabled={!ready} onChange={(e) => setVfs(e.currentTarget.value)} />
          </Field>
          <Field label="录入口径">
            <Select
              value={mode}
              disabled={!ready}
              onChange={(e) => setMode(e.currentTarget.value as "volt" | "code")}
            >
              <option value="volt">电压 V</option>
              <option value="code">理想码 Code</option>
            </Select>
          </Field>
          <Field label="通道">
            <Select value={ch} disabled={!ready} onChange={(e) => setCh(Number(e.currentTarget.value))}>
              {[0, 1, 2, 3].map((c) => (
                <option key={c} value={c}>
                  ch{c}（{manual?.channels[c]?.points.length ?? 0} 点）
                </option>
              ))}
            </Select>
          </Field>
        </div>

        {boardDir === "ad" ? (
          <div className="mt-4 flex flex-col gap-3">
            <div className="flex flex-wrap items-center gap-3">
              <Button size="sm" disabled={!connected || !isManualFw} onClick={() => run(() => api.manualReadAd(1))}>
                读当前码
              </Button>
              <span className="font-mono tabular-nums text-xs text-ink-soft">
                {(report?.ad_read ?? [null, null, null, null]).map((v, i) => `ch${i}:${v ?? "—"}`).join("  ")}
              </span>
            </div>
            <div className="flex flex-wrap items-center gap-3">
              <Field label={mode === "volt" ? "施加电压" : "施加码"} suffix={mode === "volt" ? "V" : ""}>
                <Input className="w-24" value={value} disabled={!ready} onChange={(e) => setValue(e.currentTarget.value)} />
              </Field>
              <span className="font-mono tabular-nums text-xs text-ink-faint">ideal={ideal}</span>
              <Button
                size="sm"
                disabled={!ready || samplingCh !== null}
                onClick={() => run(() => api.manualAdd(ch, ideal))}
              >
                采集点
              </Button>
              <span className={`text-xs ${adReplaceAt !== null ? "text-warn-ink" : "text-ink-faint"}`}>
                {samplingCh !== null
                  ? `采集中 ch${samplingCh}…（完成后自动出点并刷新曲线）`
                  : adReplaceAt !== null
                    ? `ideal=${ideal} 已存在，采点将覆盖第 ${adReplaceAt.n} 点`
                    : points < MANUAL_MIN_POINTS
                      ? `ch${ch} 已采 ${points} 点，至少 ${MANUAL_MIN_POINTS} 点且跨度 ≥ 4096 才能拟合`
                      : `ch${ch} 已采 ${points} 点`}
              </span>
            </div>
          </div>
        ) : (
          <div className="mt-4 flex flex-col gap-3">
            <div className="flex flex-wrap items-center gap-3">
              <Field label="raw 码">
                <Input className="w-24" value={raw} disabled={!ready} onChange={(e) => setRaw(e.currentTarget.value)} />
              </Field>
              <Button size="sm" disabled={!ready} onClick={() => run(() => api.manualSetRaw(ch, clamp(Number(raw) || 0)))}>
                写入
              </Button>
              <span className={`text-xs ${pendingRaw !== null ? "text-accent" : "text-ink-faint"}`}>
                {pendingRaw !== null ? `已写入 raw=${pendingRaw}，待采点` : "每点先写 raw，再录实测值采点"}
              </span>
            </div>
            <div className="flex flex-wrap items-center gap-3">
              <Field label={mode === "volt" ? "实测电压" : "实测码"} suffix={mode === "volt" ? "V" : ""}>
                <Input className="w-24" value={value} disabled={!ready} onChange={(e) => setValue(e.currentTarget.value)} />
              </Field>
              <span className="font-mono tabular-nums text-xs text-ink-faint">ideal={ideal}</span>
              <Button
                size="sm"
                disabled={!ready || samplingCh !== null || pendingRaw === null}
                onClick={() => run(() => api.manualAdd(ch, ideal))}
              >
                采集点
              </Button>
              <span className={`text-xs ${replaceAt !== null ? "text-warn-ink" : "text-ink-faint"}`}>
                {replaceAt !== null
                  ? `raw=${pendingRaw} 已存在，采点将覆盖第 ${replaceAt.n} 点`
                  : `ch${ch} 已采 ${points} 点`}
              </span>
            </div>
          </div>
        )}

        {replaced && (
          <div className="mt-2 text-xs text-warn-ink">
            {boardDir === "ad"
              ? `已覆盖 ch${replaced.ch} 第 ${replaced.n} 点（ideal=${replaced.ideal}）：raw ${replaced.old} → ${replacedPoint?.raw ?? "—"}`
              : `已覆盖 ch${replaced.ch} 第 ${replaced.n} 点：ideal ${replaced.old} → ${replacedPoint?.ideal ?? replaced.ideal}`}
          </div>
        )}

        <div className="mt-4 flex flex-wrap items-center gap-3">
          <Button size="sm" disabled={!ready || !anyPoints} onClick={() => run(api.manualFit)}>
            拟合
          </Button>
          <Button
            variant="primary"
            size="sm"
            disabled={!ready || fittedChs.length === 0}
            onClick={() =>
              run(async () => {
                await ctl.apply();
                await api.manualSave();
              })
            }
          >
            写入 Flash
          </Button>
          {fittedChs.length > 0 && (
            <span className="text-xs text-ink-soft">将写入 ch{fittedChs.join(", ch")}</span>
          )}
          {channel?.resid && (
            <span className="font-mono tabular-nums text-xs text-ink-soft">
              残差 max={channel.resid.max.toFixed(2)} rmse={channel.resid.rmse.toFixed(2)}
            </span>
          )}
        </div>

        {error && <div className="mt-3 text-xs text-danger-ink">{error}</div>}
      </Card>

      <ManualChart report={report} ch={ch} boardDir={boardDir} />

      {manual && manual.errors.length > 0 && (
        <Card>
          <CardTitle>手动校准错误 ({manual.errors.length})</CardTitle>
          <ul className="max-h-40 space-y-0.5 overflow-auto font-mono text-xs text-danger-ink">
            {manual.errors.map((e, i) => (
              <li key={i}>{e}</li>
            ))}
          </ul>
        </Card>
      )}

      <Card>
        <CardTitle>采样点 · ch{ch}</CardTitle>
        <div className="mb-2 text-xs text-ink-faint">
          {boardDir === "ad"
            ? "AD 以施加 ideal 为键：同一 ideal 再采会覆盖该点 raw"
            : "DA 以写入 raw 为键：同一 raw 再采会覆盖该点 ideal"}
        </div>
        {(channel?.points.length ?? 0) === 0 ? (
          <div className="text-xs text-ink-faint">暂无采样点</div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-[13px] whitespace-nowrap">
              <thead className="text-xs text-ink-faint">
                <tr>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">n</th>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">ideal</th>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">raw</th>
                </tr>
              </thead>
              <tbody>
                {channel?.points.map((p) => (
                  <tr key={p.n} className="border-t border-line font-mono tabular-nums">
                    <td className="px-3 py-2">{p.n}</td>
                    <td className="px-3 py-2">{p.ideal}</td>
                    <td className="px-3 py-2">{p.raw}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </Card>

      {manual && (manual.channels.some((c) => c.resid) || manual.channels.some((c) => c.verify)) && (
        <Card>
          <CardTitle>拟合 / 复检</CardTitle>
          <div className="overflow-x-auto">
            <table className="w-full text-[13px] whitespace-nowrap">
              <thead className="text-xs text-ink-faint">
                <tr>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">通道</th>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">拟合</th>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">残差 max/rmse</th>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">复检 max/rmse</th>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">系数 c0..c3</th>
                </tr>
              </thead>
              <tbody>
                {manual.channels.map((c) => (
                  <tr key={c.ch} className="border-t border-line">
                    <td className="px-3 py-2 font-mono">ch{c.ch}</td>
                    <td className="px-3 py-2">
                      <Pill label={c.fitted ? "已拟合" : "未拟合"} tone={c.fitted ? "green" : "slate"} />
                    </td>
                    <td className="px-3 py-2 font-mono tabular-nums">
                      {c.resid ? `${c.resid.max.toFixed(2)} / ${c.resid.rmse.toFixed(2)}` : "—"}
                    </td>
                    <td className="px-3 py-2 font-mono tabular-nums">
                      {c.verify ? `${c.verify.max.toFixed(2)} / ${c.verify.rmse.toFixed(2)}` : "—"}
                    </td>
                    <td className="px-3 py-2 font-mono text-ink-soft">
                      {c.coeff ? c.coeff.map((v) => String(v)).join(", ") : "—"}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </Card>
      )}
    </div>
  );
}
