import { useState } from "react";
import type { CalibReport } from "../types";
import { Card, CardTitle, Pill } from "./ui";

const W = 860;
const H = 340;
const PAD_L = 60;
const PAD_R = 16;
const PAD_T = 20;
const PAD_B = 30;
const CODE_MAX = 65535;

const horner = (c: [number, number, number, number], x: number) =>
  ((c[3] * x + c[2]) * x + c[1]) * x + c[0];

const sx = (v: number) => PAD_L + (v / CODE_MAX) * (W - PAD_L - PAD_R);
const sy = (v: number) => H - PAD_B - (v / CODE_MAX) * (H - PAD_T - PAD_B);

export function ManualChart({
  report,
  ch,
  boardDir,
}: {
  report: CalibReport | null;
  ch: number;
  boardDir: string | null;
}) {
  const [hover, setHover] = useState<number | null>(null);
  const channel = report?.manual.channels[ch];
  const points = channel?.points ?? [];
  const samplingCh = report?.manual.sampling_ch ?? null;
  const isAd = boardDir === "ad";
  // AD: x=raw y=ideal（拟合 ideal=f(raw)）；DA: x=ideal y=raw（拟合 raw=f(ideal)）
  const xy = points.map((p) => (isAd ? { x: p.raw, y: p.ideal } : { x: p.ideal, y: p.raw }));
  const coeff = channel?.coeff ?? null;
  const curve: string[] = [];
  if (coeff && xy.length >= 2) {
    const xs = xy.map((p) => p.x);
    const x0 = Math.min(...xs);
    const x1 = Math.max(...xs);
    const n = 64;
    for (let i = 0; i <= n; i++) {
      const x = x0 + ((x1 - x0) * i) / n;
      const y = horner(coeff, x);
      curve.push(`${sx(x)},${sy(Math.max(0, Math.min(CODE_MAX, y)))}`);
    }
  }
  const rep = report?.manual.last_replace;
  const replacedIdx = rep && rep.ch === ch ? points.findIndex((p) => p.n === rep.n) : -1;
  const focusIdx = replacedIdx >= 0 ? replacedIdx : xy.length - 1;
  const shownIdx = hover !== null && xy[hover] ? hover : focusIdx;
  const shown = shownIdx >= 0 ? xy[shownIdx] : null;
  const shownIdeal = shown ? (isAd ? shown.y : shown.x) : 0;
  const shownRaw = shown ? (isAd ? shown.x : shown.y) : 0;
  const xLabel = isAd ? "ADC raw" : "实测 ideal";
  const yLabel = isAd ? "施加 ideal" : "写入 raw";

  const onMove = (e: React.MouseEvent<SVGSVGElement>) => {
    if (xy.length === 0) return;
    const rect = e.currentTarget.getBoundingClientRect();
    const xv = ((e.clientX - rect.left) / rect.width) * W;
    const value = ((xv - PAD_L) / (W - PAD_L - PAD_R)) * CODE_MAX;
    let best = 0;
    for (let i = 1; i < xy.length; i++) {
      if (Math.abs(xy[i].x - value) < Math.abs(xy[best].x - value)) best = i;
    }
    setHover(best);
  };

  return (
    <Card>
      <CardTitle
        right={
          <div className="flex items-center gap-2">
            <Pill label={`${points.length} 点`} tone="slate" />
            {channel?.fitted ? <Pill label="已拟合" tone="green" /> : null}
            {samplingCh !== null ? <Pill label={`采集中 ch${samplingCh}`} tone="amber" /> : null}
          </div>
        }
      >
        手动测量曲线 · ch{ch}
      </CardTitle>
      <div style={{ aspectRatio: `${W}/${H}` }} className="w-full">
        <svg viewBox={`0 0 ${W} ${H}`} className="h-full w-full" onMouseMove={onMove} onMouseLeave={() => setHover(null)}>
          {[0, 16384, 32768, 49152, 65535].map((v) => (
            <g key={v}>
              <line x1={sx(v)} y1={PAD_T} x2={sx(v)} y2={H - PAD_B} stroke="#e5e5ea" strokeWidth={1} />
              <line x1={PAD_L} y1={sy(v)} x2={W - PAD_R} y2={sy(v)} stroke="#e5e5ea" strokeWidth={1} />
              <text x={sx(v)} y={H - PAD_B + 14} textAnchor="middle" fontSize={10} fill="#8e8e93">
                {v}
              </text>
              <text x={PAD_L - 6} y={sy(v) + 3} textAnchor="end" fontSize={10} fill="#8e8e93">
                {v}
              </text>
            </g>
          ))}
          <text x={(PAD_L + W - PAD_R) / 2} y={H - 4} textAnchor="middle" fontSize={11} fill="#636366">
            {xLabel}
          </text>
          <text
            x={14}
            y={(PAD_T + H - PAD_B) / 2}
            textAnchor="middle"
            fontSize={11}
            fill="#636366"
            transform={`rotate(-90 14 ${(PAD_T + H - PAD_B) / 2})`}
          >
            {yLabel}
          </text>

          {curve.length > 1 && (
            <polyline points={curve.join(" ")} fill="none" stroke="#0a84ff" strokeWidth={1.8} strokeLinejoin="round" />
          )}

          {xy.map((p, i) => {
            const isFocus = i === focusIdx;
            const active = hover === i;
            return (
              <circle
                key={i}
                cx={sx(p.x)}
                cy={sy(p.y)}
                r={active ? 4.5 : isFocus ? 4 : 2.6}
                fill={isFocus ? "#ff9f0a" : "#0a84ff"}
                stroke={active ? "#1c1c1e" : "none"}
                strokeWidth={active ? 1.2 : 0}
              />
            );
          })}

          {xy.length === 0 && (
            <text x={(PAD_L + W - PAD_R) / 2} y={(PAD_T + H - PAD_B) / 2} textAnchor="middle" fontSize={12} fill="#8e8e93">
              暂无采样点：每次「采集点」后立即刷新
            </text>
          )}
        </svg>
      </div>
      <div className="mt-2 flex h-5 items-center gap-4 overflow-hidden font-mono text-xs tabular-nums text-ink-soft">
        {shown ? (
          <>
            <span className={hover === null ? "text-ink" : "text-ink-faint"}>
              {hover === null ? (replacedIdx >= 0 ? "覆盖" : "最新") : "选中"}
            </span>
            <span>#{points[shownIdx]?.n ?? shownIdx + 1}</span>
            <span>ideal={shownIdeal}</span>
            <span>raw={shownRaw}</span>
            {coeff ? <span>拟合={horner(coeff, shown.x).toFixed(1)}</span> : <span>未拟合</span>}
          </>
        ) : (
          <span className="text-ink-faint">—</span>
        )}
      </div>
    </Card>
  );
}
