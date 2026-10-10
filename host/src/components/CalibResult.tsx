import type { CalibReport, Stat } from "../types";

interface Props {
  report: CalibReport | null;
}

const num = (v: number | null | undefined, digits = 2) =>
  v === null || v === undefined ? "—" : v.toFixed(digits);

const stat = (s: Stat | null) => `${num(s?.max)} / ${num(s?.rmse)}`;

function Badge({ label, tone }: { label: string; tone: "green" | "red" | "gray" }) {
  const cls =
    tone === "green"
      ? "bg-green-600/20 text-green-400 border-green-600"
      : tone === "red"
        ? "bg-red-600/20 text-red-400 border-red-600"
        : "bg-slate-600/20 text-slate-400 border-slate-600";
  return <span className={`rounded border px-2 py-0.5 text-xs ${cls}`}>{label}</span>;
}

export function CalibResult({ report }: Props) {
  if (!report) {
    return <div className="p-4 text-sm text-slate-500">无校准数据</div>;
  }

  const verifyTone = report.verify_pass === true ? "green" : report.verify_pass === false ? "red" : "gray";
  const verifyLabel =
    report.verify_pass === true ? "复检 PASS" : report.verify_pass === false ? "复检 FAIL" : "未复检";

  return (
    <div className="flex flex-col gap-3 p-4">
      <div className="flex flex-wrap items-center gap-2 text-sm">
        <span className="font-semibold text-slate-200">校准结果</span>
        <Badge label={`角色 ${report.role ?? "—"}`} tone="gray" />
        <Badge label={`阶段 ${report.phase}`} tone="gray" />
        <Badge label={`点 ${report.points}/${report.total_points}`} tone="gray" />
        <Badge label={report.saved ? "已落盘" : "未落盘"} tone={report.saved ? "green" : "gray"} />
        {report.verify_started && <Badge label={verifyLabel} tone={verifyTone} />}
      </div>

      <table className="w-full border-collapse text-xs">
        <thead>
          <tr className="text-left text-slate-400">
            <th className="border-b border-slate-700 px-2 py-1">通道</th>
            <th className="border-b border-slate-700 px-2 py-1">前误差 max/rmse</th>
            <th className="border-b border-slate-700 px-2 py-1">拟合残差 max/rmse</th>
            <th className="border-b border-slate-700 px-2 py-1">复检 max/rmse</th>
            <th className="border-b border-slate-700 px-2 py-1">系数 c0..c3</th>
          </tr>
        </thead>
        <tbody>
          {report.channels.map((ch) => (
            <tr key={ch.ch} className="text-slate-200">
              <td className="border-b border-slate-800 px-2 py-1 font-mono">ch{ch.ch}</td>
              <td className="border-b border-slate-800 px-2 py-1 font-mono">{stat(ch.before)}</td>
              <td className="border-b border-slate-800 px-2 py-1 font-mono">{stat(ch.resid)}</td>
              <td className="border-b border-slate-800 px-2 py-1 font-mono">{stat(ch.verify)}</td>
              <td className="border-b border-slate-800 px-2 py-1 font-mono text-slate-400">
                {ch.coeff ? ch.coeff.map((c) => num(c, 6)).join(", ") : "—"}
              </td>
            </tr>
          ))}
        </tbody>
      </table>

      {report.errors.length > 0 && (
        <div className="rounded border border-red-800 bg-red-950/40 p-2">
          <div className="mb-1 text-xs font-semibold text-red-400">错误 ({report.errors.length})</div>
          <ul className="max-h-32 space-y-0.5 overflow-auto font-mono text-xs text-red-300">
            {report.errors.map((e, i) => (
              <li key={i}>{e}</li>
            ))}
          </ul>
        </div>
      )}
    </div>
  );
}
