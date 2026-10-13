import { useRef, useState } from "react";
import type { CalibReport } from "../types";
import { Card, Segmented } from "./ui";

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

export function CalibChart({ report }: { report: CalibReport | null }) {
  const [mode, setMode] = useState<"sweep" | "verify">("sweep");
  const [activeCh, setActiveCh] = useState(0);
  const [hover, setHover] = useState<number | null>(null);
  const svgRef = useRef<SVGSVGElement>(null);

  const showBefore = mode === "sweep";
  const grid = (mode === "verify" ? report?.verify_grid : report?.grid) ?? [];
  const channel = report?.channels[activeCh];
  const samples = (mode === "verify" ? channel?.verify_samples : channel?.samples) ?? [];
  const valid = grid.length > 1 && samples.length === grid.length;

  const before = valid ? samples.map((s, i) => s - grid[i]) : [];
  const after =
    valid && channel?.coeff ? samples.map((s, i) => horner(channel.coeff!, s) - grid[i]) : [];

  const drawn = showBefore ? [...before, ...after] : after;
  const maxAbs = Math.max(16, ...drawn.map(Math.abs));
  const ymax = maxAbs * 1.15;
  const sy = (err: number) => H / 2 - (err / ymax) * (H / 2 - PAD_T);

  const line = (values: number[]) => values.map((e, i) => `${sx(grid[i])},${sy(e)}`).join(" ");

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
    { v: 16, color: "#c9c9ce" },
    { v: -16, color: "#c9c9ce" },
    { v: 1000, color: "#ffb340" },
    { v: -1000, color: "#ffb340" },
  ].filter((r) => Math.abs(r.v) <= ymax);

  return (
    <Card className="flex flex-col gap-2">
      <div className="flex flex-wrap items-center gap-3">
        <span className="text-[13px] font-semibold text-ink">{showBefore ? "误差曲线" : "误差曲线 (verify)"}</span>
        <Segmented
          options={[
            { value: 0, label: "扫描误差" },
            { value: 1, label: "复检误差" },
          ]}
          value={mode === "sweep" ? 0 : 1}
          onChange={(v) => {
            setMode(v === 0 ? "sweep" : "verify");
            setHover(null);
          }}
        />
        <Segmented
          options={[0, 1, 2, 3].map((c) => ({ value: c, label: `ch${c}` }))}
          value={activeCh}
          onChange={(v) => {
            setActiveCh(v);
            setHover(null);
          }}
        />
        <span className="flex items-center gap-1 text-xs text-ink-soft">
          {showBefore ? (
            <>
              <span className="inline-block h-0.5 w-4 bg-[#8e8e93]" /> 前
              <span className="ml-2 inline-block h-0.5 w-4 bg-[#007aff]" /> 后
            </>
          ) : (
            <>
              <span className="inline-block h-0.5 w-4 bg-[#007aff]" /> 误差
            </>
          )}
        </span>
      </div>

      {valid ? (
        <>
          <div className="h-5 font-mono tabular-nums text-ink-soft">
            {hover === null
              ? "移动鼠标查看逐点误差"
              : mode === "verify"
                ? `set=${grid[hover]} 误差=${after.length ? after[hover] : "—"}`
                : `ideal=${grid[hover]} 前=${before[hover]} 后=${after.length ? after[hover] : "—"}`}
          </div>
          <svg
            ref={svgRef}
            viewBox={`0 0 ${W} ${H}`}
            className="w-full rounded-lg border border-line bg-panel"
            onMouseMove={onMove}
            onMouseLeave={() => setHover(null)}
          >
            <line x1={PAD_L} y1={sy(0)} x2={W - PAD_R} y2={sy(0)} stroke="#d2d2d7" />
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
                <line x1={sx(t)} y1={PAD_T} x2={sx(t)} y2={H - PAD_B} stroke="#f0f0f2" />
                <text x={sx(t)} y={H - 8} fill="#86868b" fontSize={10} textAnchor="middle">
                  {t}
                </text>
              </g>
            ))}
            <text x={PAD_L - 8} y={sy(ymax) + 10} fill="#86868b" fontSize={10} textAnchor="end">
              +{ymax.toFixed(0)}
            </text>
            <text x={PAD_L - 8} y={sy(-ymax)} fill="#86868b" fontSize={10} textAnchor="end">
              -{ymax.toFixed(0)}
            </text>
            {after.length > 0 && (
              <polyline
                points={line(after)}
                fill="none"
                stroke="#007aff"
                strokeWidth={1.4}
                strokeLinejoin="round"
              />
            )}
            {showBefore && (
              <polyline points={line(before)} fill="none" stroke="#8e8e93" strokeWidth={1.4} strokeLinejoin="round" />
            )}
            {hover !== null && (
              <line
                x1={sx(grid[hover])}
                y1={PAD_T}
                x2={sx(grid[hover])}
                y2={H - PAD_B}
                stroke="#ff9f0a"
                strokeWidth={0.8}
              />
            )}
          </svg>
        </>
      ) : (
        <div className="p-6 text-ink-faint">无采样数据</div>
      )}
    </Card>
  );
}
