import { useRef, useState } from "react";
import type { CalibReport, RetryEvent } from "../types";
import { Card, Segmented, Switch } from "./ui";

const W = 860;
const H = 340;
const PAD_L = 52;
const PAD_R = 16;
const PAD_T = 20;
const PAD_B = 28;
const CODE_MAX = 65535;

const RETRY_COLOR = "#ff9f0a";
const VERIFY_COLOR = "#30a46c";

const horner = (c: [number, number, number, number], x: number) =>
  ((c[3] * x + c[2]) * x + c[1]) * x + c[0];

const sx = (code: number) => PAD_L + (code / CODE_MAX) * (W - PAD_L - PAD_R);

export function CalibChart({ report }: { report: CalibReport | null }) {
  const [mode, setMode] = useState<"sweep" | "verify">("sweep");
  const [activeCh, setActiveCh] = useState(0);
  const [hover, setHover] = useState<number | null>(null);
  const [overlayVerify, setOverlayVerify] = useState(true);
  const svgRef = useRef<SVGSVGElement>(null);

  const showBefore = mode === "sweep";
  const grid = (mode === "verify" ? report?.verify_grid : report?.grid) ?? [];
  const channel = report?.channels[activeCh];
  const samples = (mode === "verify" ? channel?.verify_samples : channel?.samples) ?? [];
  const valid = grid.length > 1 && samples.length === grid.length;

  const before = valid ? samples.map((s, i) => s - grid[i]) : [];
  const after =
    valid && channel?.coeff ? samples.map((s, i) => horner(channel.coeff!, s) - grid[i]) : [];

  // 复检叠加（仅扫描图）：同一通道的复检点与复检误差，直接用落盘系数求值
  const vGrid = report?.verify_grid ?? [];
  const vSamples = channel?.verify_samples ?? [];
  const verifyOn =
    mode === "sweep" && overlayVerify && vGrid.length > 1 && vSamples.length === vGrid.length;
  const vAfter =
    verifyOn && channel?.coeff ? vSamples.map((s, i) => horner(channel.coeff!, s) - vGrid[i]) : [];

  // 坏点重测标记：本次修订（不拖坏合格点、可能保留旧样本）
  const retries = report?.retries ?? [];
  const sweepEvs = mode === "sweep" ? retries.filter((e) => e.ch === activeCh && !e.verify) : [];
  const verifyEvs = retries.filter(
    (e) => e.ch === activeCh && e.verify && (mode === "verify" || verifyOn),
  );
  const retryRounds = Math.max(0, ...sweepEvs.map((e) => e.round), ...verifyEvs.map((e) => e.round));

  const drawn = showBefore ? [...before, ...after, ...vAfter] : [...after];
  const maxAbs = Math.max(16, ...drawn.map(Math.abs));
  const ymax = maxAbs * 1.15;
  const sy = (err: number) => H / 2 - (err / ymax) * (H / 2 - PAD_T);
  const syClamp = (err: number) => Math.max(PAD_T, Math.min(H - PAD_B, sy(err)));

  const line = (values: number[]) => values.map((e, i) => `${sx(grid[i])},${sy(e)}`).join(" ");
  const vLine = (values: number[]) => values.map((e, i) => `${sx(vGrid[i])},${sy(e)}`).join(" ");

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

  const hoverEvs = (
    hover === null ? [] : [...sweepEvs, ...verifyEvs].filter((e) => e.idx === hover)
  ).sort((a, b) => a.round - b.round);

  const retryText = (e: RetryEvent) =>
    `重测${e.round}轮 ${e.dropped ? "保留旧值" : "采纳"}(新=${e.d.toFixed(1)} 旧=${e.old.toFixed(1)})`;

  const markerX = (e: RetryEvent) => sx((e.verify ? vGrid : grid)[e.idx] ?? e.at);

  const markers = [...sweepEvs, ...verifyEvs];

  return (
    <Card className="flex flex-col gap-2">
      <div className="flex flex-wrap items-center gap-3">
        <span className="text-[13px] font-semibold text-ink">
          {showBefore ? "误差曲线" : "误差曲线 (verify)"}
        </span>
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
        {mode === "sweep" && (
          <Switch checked={overlayVerify} onChange={setOverlayVerify} label="叠加复检" />
        )}
      </div>

      {/* 图例行与读数行常驻定高：重测/复检数据到达时不得让卡片高度变化（高度变化会把滚动位置钳制到顶部） */}
      <div className="flex h-5 items-center gap-1 overflow-hidden text-xs text-ink-soft">
        {showBefore ? (
          <>
            <span className="inline-block h-0.5 w-4 shrink-0 bg-[#8e8e93]" /> 前
            <span className="ml-2 inline-block h-0.5 w-4 shrink-0 bg-[#007aff]" /> 后
          </>
        ) : (
          <>
            <span className="inline-block h-0.5 w-4 shrink-0 bg-[#007aff]" /> 误差
          </>
        )}
        {verifyOn && (
          <>
            <span className="ml-2 inline-block h-0 w-4 shrink-0 border-t border-dashed border-[#30a46c]" /> 复检
          </>
        )}
        {markers.length > 0 && (
          <>
            <span className="ml-2 inline-block h-2 w-2 shrink-0 rounded-full bg-[#ff9f0a]" /> 重测 {retryRounds} 轮 /{" "}
            {markers.length} 点（保留旧值 {markers.filter((e) => e.dropped).length}）
          </>
        )}
      </div>

      <div className="h-5 overflow-hidden font-mono tabular-nums text-ink-soft">
        {!valid
          ? "无采样数据"
          : hover === null
            ? "移动鼠标查看逐点误差"
            : mode === "verify"
              ? `set=${grid[hover]} 误差=${after.length ? after[hover] : "—"}${
                  hoverEvs.length ? ` ${hoverEvs.map(retryText).join(" / ")}` : ""
                }`
              : `ideal=${grid[hover]} 前=${before[hover]} 后=${after.length ? after[hover] : "—"}${
                  hoverEvs.length ? ` ${hoverEvs.map(retryText).join(" / ")}` : ""
                }`}
      </div>

      {/* 固定宽高比容器：有无数据时高度一致，数据刷新不会让页面高度塌缩引起滚动跳顶 */}
      <div className="relative w-full" style={{ aspectRatio: `${W} / ${H}` }}>
        <svg
          ref={svgRef}
          viewBox={`0 0 ${W} ${H}`}
          className="h-full w-full rounded-lg border border-line bg-panel"
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
          {vAfter.length > 0 && (
            <polyline
              points={vLine(vAfter)}
              fill="none"
              stroke={VERIFY_COLOR}
              strokeWidth={1.2}
              strokeDasharray="5 3"
              strokeLinejoin="round"
            />
          )}
          {markers.map((e, i) => (
            <g key={`r${i}`}>
              {/* 旧样本偏差 → 重测偏差：竖线示意改善量 */}
              <line
                x1={markerX(e)}
                y1={syClamp(e.old)}
                x2={markerX(e)}
                y2={syClamp(e.d)}
                stroke={RETRY_COLOR}
                strokeWidth={1}
                strokeOpacity={0.7}
              />
              <circle
                cx={markerX(e)}
                cy={syClamp(e.d)}
                r={3}
                fill={e.dropped ? "none" : RETRY_COLOR}
                stroke={RETRY_COLOR}
                strokeWidth={1.2}
              >
                <title>{retryText(e)}</title>
              </circle>
            </g>
          ))}
          {valid && hover !== null && (
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
      </div>
    </Card>
  );
}
