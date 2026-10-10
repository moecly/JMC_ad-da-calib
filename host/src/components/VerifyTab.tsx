import { useState } from "react";
import type { CalibReport } from "../types";
import * as api from "../lib/ipc";
import { Button, Card, CardTitle, Pill, type Tone } from "./ui";
import { ChannelLinkMatrix } from "./ChannelLinkMatrix";
import { ErrorChart } from "./ErrorChart";

const num = (v: number | null | undefined, digits = 2) =>
  v === null || v === undefined ? "—" : v.toFixed(digits);

const boardLabel = (dir: string | null | undefined) =>
  dir === "ad" ? "AD 采集板" : dir === "da" ? "DA 输出板" : "—";

export function VerifyTab({ connected, report }: { connected: boolean; report: CalibReport | null }) {
  const [enabled, setEnabled] = useState<boolean[]>([false, false, false, false]);
  const [daSel, setDaSel] = useState<number[]>([0, 0, 0, 0]);
  const [error, setError] = useState<string | null>(null);

  const run = (fn: () => Promise<void>) => {
    setError(null);
    fn().catch((e) => setError(String(e)));
  };

  const maskOf = () => enabled.reduce((m, on, ch) => (on ? m | (1 << ch) : m), 0);

  const dutIsDa = report?.board_dir === "da";
  const dutName = dutIsDa ? "DA" : "AD";
  const refName = dutIsDa ? "AD" : "DA";
  const isDut = report?.role === "DUT";

  const verifyTone: Tone = report?.verify_pass === true ? "green" : report?.verify_pass === false ? "red" : "slate";
  const verifyLabel =
    report?.verify_pass === true ? "复检 PASS" : report?.verify_pass === false ? "复检 FAIL" : "未复检";

  return (
    <div className="flex flex-col gap-4">
      <Card>
        <CardTitle right={<Pill label={isDut ? "DUT" : (report?.role ?? "—")} tone={isDut ? "blue" : "slate"} />}>
          复检
        </CardTitle>

        {!isDut && (
          <div className="mb-3 rounded-lg bg-warn-soft px-3 py-2 text-xs text-warn-ink">
            {`复检需要 DUT 固件（CALIB_ROLE=DUT）。当前角色 ${report?.role ?? "未知"}；请烧录 DUT 固件后在「概览 → 刷新信息」确认。`}
          </div>
        )}
        {report && !report.record_loaded && (
          <div className="mb-3 rounded-lg bg-warn-soft px-3 py-2 text-xs text-warn-ink">
            本机无校准记录（Flash 未加载），复检将按全零系数计算，结果不可用。
          </div>
        )}

        <ChannelLinkMatrix
          enabled={enabled}
          daSel={daSel}
          dutName={dutName}
          refName={refName}
          disabled={!connected || !isDut}
          onToggle={(ch, on) => setEnabled((p) => p.map((x, i) => (i === ch ? on : x)))}
          onDaSelect={(ch, da) => setDaSel((p) => p.map((x, i) => (i === ch ? da : x)))}
        />

        <div className="mt-4 flex flex-wrap items-center gap-2">
          <Button
            variant="primary"
            size="sm"
            disabled={!connected || !isDut || maskOf() === 0}
            onClick={() => run(() => api.calibVerify(maskOf(), daSel))}
          >
            开始复检
          </Button>
          <Button size="sm" disabled={!connected} onClick={() => run(api.calibStop)}>
            停止
          </Button>
          <Button size="sm" disabled={!connected} onClick={() => run(api.calibStatus)}>
            刷新信息
          </Button>
        </div>

        <div className="mt-3 flex flex-wrap items-center gap-4 text-xs text-ink-soft">
          <span>
            阶段 <span className="font-mono text-ink">{report?.phase ?? "—"}</span>
          </span>
          <span>
            点数{" "}
            <span className="font-mono text-ink">
              {report?.verify_points ?? 0}/{report?.total_points ?? 67}
            </span>
          </span>
          <span>
            REF <span className="font-mono text-ink">{report?.ref ? (report.ref.no_reply ? "无应答" : boardLabel(report.ref.board)) : "—"}</span>
          </span>
        </div>

        {error && <div className="mt-3 text-xs text-danger-ink">{error}</div>}
      </Card>

      {report && report.verify_points > 0 && (
        <Card>
          <CardTitle right={<Pill label={verifyLabel} tone={verifyTone} />}>复检结果</CardTitle>
          <div className="overflow-x-auto">
            <table className="w-full text-[13px]">
              <thead className="text-xs text-ink-faint">
                <tr>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">通道</th>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">复检 max</th>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">@set</th>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">rmse</th>
                  <th className="px-3 py-2 text-left font-medium whitespace-nowrap">系数 c0..c3</th>
                </tr>
              </thead>
              <tbody>
                {report.channels.map((ch) => (
                  <tr key={ch.ch} className="border-t border-line">
                    <td className="px-3 py-2 font-mono">ch{ch.ch}</td>
                    <td className="px-3 py-2 font-mono tabular-nums">{ch.verify ? num(ch.verify.max) : "—"}</td>
                    <td className="px-3 py-2 font-mono tabular-nums">{ch.verify_at ?? "—"}</td>
                    <td className="px-3 py-2 font-mono tabular-nums">{ch.verify ? num(ch.verify.rmse) : "—"}</td>
                    <td className="px-3 py-2 font-mono text-ink-soft">
                      {ch.coeff ? ch.coeff.map((c) => num(c, 6)).join(", ") : "—"}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </Card>
      )}

      {report && report.verify_points > 0 && <ErrorChart report={report} mode="verify" />}

      {report && report.errors.length > 0 && (
        <Card>
          <CardTitle>错误与警告 ({report.errors.length})</CardTitle>
          <ul className="max-h-40 space-y-0.5 overflow-auto font-mono text-xs text-danger-ink">
            {report.errors.map((e, i) => (
              <li key={i}>{e}</li>
            ))}
          </ul>
        </Card>
      )}
    </div>
  );
}
