import type { CalibReport, Stat } from "../types";
import { Card, CardTitle, Pill, type Tone } from "./ui";

interface Props {
  report: CalibReport | null;
}

const num = (v: number | null | undefined, digits = 2) =>
  v === null || v === undefined ? "—" : v.toFixed(digits);

const stat = (s: Stat | null) => `${num(s?.max)} / ${num(s?.rmse)}`;

export function CalibResult({ report }: Props) {
  if (!report) {
    return (
      <Card>
        <CardTitle>校准结果</CardTitle>
        <div className="text-ink-faint">无校准数据</div>
      </Card>
    );
  }

  const verifyTone: Tone = report.verify_pass === true ? "green" : report.verify_pass === false ? "red" : "slate";
  const verifyLabel =
    report.verify_pass === true ? "复检 PASS" : report.verify_pass === false ? "复检 FAIL" : "未复检";

  return (
    <Card>
      <CardTitle
        right={
          <div className="flex flex-wrap items-center gap-2">
            <Pill label={`角色 ${report.role ?? "—"}`} tone="slate" />
            <Pill label={`阶段 ${report.phase}`} tone="slate" />
            <Pill label={`点 ${report.points}/${report.total_points}`} tone="slate" />
            <Pill label={report.saved ? "已落盘" : "未落盘"} tone={report.saved ? "green" : "slate"} />
            {report.verify_skipped ? (
              <Pill label="复检 已跳过" tone="amber" />
            ) : (
              report.verify_started && <Pill label={verifyLabel} tone={verifyTone} />
            )}
          </div>
        }
      >
        校准结果
      </CardTitle>

      <div className="overflow-x-auto">
        <table className="w-full text-[13px] whitespace-nowrap">
          <thead className="text-xs text-ink-faint">
            <tr>
              <th className="px-3 py-2 text-left font-medium whitespace-nowrap">通道</th>
              <th className="px-3 py-2 text-left font-medium whitespace-nowrap">状态</th>
              <th className="px-3 py-2 text-left font-medium whitespace-nowrap">前误差 max/rmse</th>
              <th className="px-3 py-2 text-left font-medium whitespace-nowrap">拟合残差 max/rmse</th>
              <th className="px-3 py-2 text-left font-medium whitespace-nowrap">复检 max/rmse</th>
              <th className="px-3 py-2 text-left font-medium whitespace-nowrap">系数 c0..c3</th>
            </tr>
          </thead>
          <tbody>
            {report.channels.map((ch) => (
              <tr key={ch.ch} className="border-t border-line">
                <td className="px-3 py-2 font-mono">ch{ch.ch}</td>
                <td className="px-3 py-2">
                  <Pill label={ch.valid ? "已校准" : "未校准"} tone={ch.valid ? "green" : "slate"} />
                </td>
                <td className="px-3 py-2 font-mono tabular-nums">{stat(ch.before)}</td>
                <td className="px-3 py-2 font-mono tabular-nums">{stat(ch.resid)}</td>
                <td className="px-3 py-2 font-mono tabular-nums">{stat(ch.verify)}</td>
                <td className="px-3 py-2 font-mono whitespace-nowrap text-ink-soft">
                  {ch.coeff ? ch.coeff.map((c) => num(c, 6)).join(", ") : "—"}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {report.errors.length > 0 && (
        <div className="mt-3 rounded-lg bg-danger-soft p-3">
          <div className="mb-1 text-xs font-semibold text-danger-ink">错误 ({report.errors.length})</div>
          <ul className="max-h-32 space-y-0.5 overflow-auto font-mono text-xs text-danger-ink">
            {report.errors.map((e, i) => (
              <li key={i}>{e}</li>
            ))}
          </ul>
        </div>
      )}
    </Card>
  );
}
