import { useRef, useState } from "react";
import type { CalibReport } from "../types";
import { Card } from "./ui";

const W = 860;
const H = 340;
const PAD_L = 52;
const PAD_R = 16;
const PAD_T = 20;
const PAD_B = 28;
const CODE_MAX = 65535;

const horner = (c: [number, number, number, number], x: number) =>
  ((c[3] * x + c[2]) * x + c[1]) * x + c[0];

const sx = (code: number) => PAD_L + (code / CODE_MAX) * (W - PAD_L - PAD_R);

export function ErrorChart({ report }: { report: CalibReport | null }) {
  const [activeCh, setActiveCh] = useState(0);
  const [hover, setHover] = useState<number | null>(null);
  const svgRef = useRef<SVGSVGElement>(null);

  const grid = report?.grid ?? [];
  const channel = report?.channels[activeCh];
  const samples = channel?.samples ?? [];
  const valid = grid.length > 1 && samples.length === grid.length;

  const before = valid ? samples.map((s, i) => s - grid[i]) : [];
  const after =
    valid && channel?.coeff ? samples.map((s, i) => horner(channel.coeff!, s) - grid[i]) : [];

  const maxAbs = Math.max(16, ...before.map(Math.abs), ...after.map(Math.abs));
  const ymax = maxAbs * 1.15;
  const sy = (err: number) => H / 2 - (err / ymax) * (H / 2 - PAD_T);

  const line = (values: number[]) =>
    values.map((e, i) => `${sx(grid[i])},${sy(e)}`).join(" ");

  const onMove = (e: React.MouseEvent<SVGSVGElement>) => {
    if (!valid) return;
    const rect = e.currentTarget.getBoundingClientRect();
    const px = ((e.clientX - rect.left) / rect.width) * W;
    const ideal = ((px - PAD_L) / (W - PAD_L - PAD_R)) * CODE_MAX;
    let best = 0;
    for (let i = 1; i < grid.length; i++) {
      if (Math.abs(grid[i] - ideal) < Math.abs(grid[best] - ideal)) best = i;
    }
    setHover(best);
  };

  const refs = [
    { v: 16, color: "#64748b" },
    { v: -16, color: "#64748b" },
    { v: 1000, color: "#7c3aed" },
    { v: -1000, color: "#7c3aed" },
  ].filter((r) => Math.abs(r.v) <= ymax);

  return (
    <Card className="flex flex-col gap-2">
      <div className="flex items-center gap-2 text-sm">
        <span className="font-semibold text-slate-200">误差曲线</span>
        {[0, 1, 2, 3].map((c) => (
          <button
            key={c}
            className={`rounded px-2 py-0.5 text-xs ${
              activeCh === c ? "bg-blue-600 text-white" : "border border-slate-600 text-slate-300"
            }`}
            onClick={() => {
              setActiveCh(c);
              setHover(null);
            }}
          >
            ch{c}
          </button>
        ))}
        <span className="ml-2 flex items-center gap-1 text-xs text-slate-400">
          <span className="inline-block h-0.5 w-4 bg-slate-400" /> 前
          <span className="ml-2 inline-block h-0.5 w-4 bg-green-500" /> 后
        </span>
      </div>

      {valid ? (
        <>
          <div className="h-5 font-mono text-xs text-slate-300">
            {hover !== null
              ? `ideal=${grid[hover]} 前=${before[hover]} 后=${after.length ? after[hover] : "—"}`
              : "移动鼠标查看逐点误差"}
          </div>
          <svg
            ref={svgRef}
            viewBox={`0 0 ${W} ${H}`}
            className="w-full rounded border border-slate-800 bg-slate-950"
            onMouseMove={onMove}
            onMouseLeave={() => setHover(null)}
          >
            <line x1={PAD_L} y1={sy(0)} x2={W - PAD_R} y2={sy(0)} stroke="#334155" />
            {refs.map((r) => (
              <line
                key={r.v}
                x1={PAD_L}
                y1={sy(r.v)}
                x2={W - PAD_R}
                y2={sy(r.v)}
                stroke={r.color}
                strokeDasharray="4 4"
                strokeWidth={0.8}
              />
            ))}
            {[0, 16384, 32768, 49152, 65535].map((t) => (
              <g key={t}>
                <line x1={sx(t)} y1={PAD_T} x2={sx(t)} y2={H - PAD_B} stroke="#1e293b" />
                <text x={sx(t)} y={H - 8} fill="#64748b" fontSize={10} textAnchor="middle">
                  {t}
                </text>
              </g>
            ))}
            <text x={PAD_L - 8} y={sy(ymax) + 10} fill="#64748b" fontSize={10} textAnchor="end">
              +{ymax.toFixed(0)}
            </text>
            <text x={PAD_L - 8} y={sy(-ymax)} fill="#64748b" fontSize={10} textAnchor="end">
              -{ymax.toFixed(0)}
            </text>
            {after.length > 0 && (
              <polyline
                points={line(after)}
                fill="none"
                stroke="#22c55e"
                strokeWidth={1.4}
                strokeLinejoin="round"
              />
            )}
            <polyline points={line(before)} fill="none" stroke="#94a3b8" strokeWidth={1.4} strokeLinejoin="round" />
            {hover !== null && (
              <line
                x1={sx(grid[hover])}
                y1={PAD_T}
                x2={sx(grid[hover])}
                y2={H - PAD_B}
                stroke="#f59e0b"
                strokeWidth={0.8}
              />
            )}
          </svg>
        </>
      ) : (
        <div className="p-6 text-sm text-slate-500">无采样数据</div>
      )}
    </Card>
  );
}
