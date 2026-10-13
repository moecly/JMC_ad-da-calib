import type { PipelineStatus } from "../types";
import { Card, CardTitle, Pill } from "./ui";
import { CalibChart } from "./CalibChart";
import { CalibResult } from "./CalibResult";

interface Props {
  pipeline: PipelineStatus | null;
}

const num = (v: number | null | undefined, digits = 2) =>
  v === null || v === undefined ? "—" : v.toFixed(digits);

const boardLabel = (dir: string | null | undefined) =>
  dir === "ad" ? "AD 采集板" : dir === "da" ? "DA 输出板" : "—";

const stepLabel = (step: string) =>
  (
    {
      idle: "待机",
      flash_dut: "烧录校准固件",
      attach_calib: "附着校准固件",
      run_calib: "自动校准",
      flash_app: "烧录应用固件",
      attach_verify: "附着应用固件",
      done: "完成",
    } as Record<string, string>
  )[step] ?? step;

function progress(done: number, total: number) {
  if (total <= 0) return 0;
  return Math.round((done / total) * 100);
}

export function CalibrationPanel({ pipeline }: Props) {
  const r = pipeline?.report ?? null;
  const ref = r?.ref ?? null;

  const step = pipeline?.step;
  const attachedCalib = (step === "attach_calib" || step === "run_calib") && pipeline?.ok !== false;
  const calibFail = step === "run_calib" && pipeline?.ok === false;
  const dutTone = attachedCalib ? "green" : calibFail || pipeline?.ok === false ? "red" : "slate";
  const dutLabel = attachedCalib
    ? "已附着校准固件"
    : calibFail
      ? "校准失败"
      : pipeline?.ok === false
        ? "附着失败"
        : step
          ? stepLabel(step)
          : "待机";

  let refTone: "green" | "red" | "slate" = "slate";
  let refLabel = "未查询";
  let refNote: string | null = null;
  if (ref && !ref.no_reply) {
    refTone = "green";
    refLabel = "已连接";
  } else if (ref?.no_reply) {
    refTone = "red";
    refLabel = "REF 无应答";
    refNote = "检查 RS485 连线 / REF 上电";
  } else if (r?.errors?.some((e) => e.includes("error ref mismatch"))) {
    refTone = "red";
    refLabel = "板型不符";
  }

  const refDetail =
    ref && !ref.no_reply
      ? `${boardLabel(ref.board)} v${ref.version ?? "—"} mask 0x${ref.valid_mask.toString(16).toUpperCase()}`
      : null;

  return (
    <div className="flex flex-col gap-4">
      <Card>
        <CardTitle>连接状态</CardTitle>
        <div className="flex flex-wrap items-center justify-between gap-4 text-sm">
          <div className="flex min-w-[200px] items-center gap-2">
            <Pill label={dutLabel} tone={dutTone} />
            <span className="font-mono text-xs text-ink-soft">{r?.device ?? "—"}</span>
          </div>
          <span className="text-ink-faint">DUT ──RS485── REF</span>
          <div className="flex min-w-[200px] items-center justify-end gap-2">
            <Pill label={refLabel} tone={refTone} />
            {refDetail ? (
              <span className="font-mono text-xs text-ink-soft">{refDetail}</span>
            ) : (
              refNote && <span className="text-xs text-danger-ink">{refNote}</span>
            )}
          </div>
        </div>
      </Card>

      <div className="grid grid-cols-1 gap-4 xl:grid-cols-2">
        <Card>
          <CardTitle>DUT（本板）</CardTitle>
          <dl className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-1.5 text-[13px]">
            <dt className="text-ink-faint">设备</dt>
            <dd className="font-mono tabular-nums text-ink">{r?.device ?? "—"}</dd>
            <dt className="text-ink-faint">固件版本</dt>
            <dd className="font-mono tabular-nums text-ink">{r?.app_version ?? "—"}</dd>
            <dt className="text-ink-faint">角色</dt>
            <dd className="font-mono tabular-nums text-ink">{r?.role ?? "—"}</dd>
            <dt className="text-ink-faint">板型方向</dt>
            <dd className="text-ink">{boardLabel(r?.board_dir)}</dd>
            <dt className="text-ink-faint">阶段</dt>
            <dd className="font-mono tabular-nums text-ink">{r?.phase ?? "—"}</dd>
            <dt className="text-ink-faint">扫描点数</dt>
            <dd className="font-mono tabular-nums text-ink">
              {r ? `${r.points}/${r.total_points}` : "—"}
            </dd>
            <dd className="col-start-2 pb-0.5">
              <div className="h-1.5 rounded bg-field">
                <div
                  className="h-full rounded bg-accent"
                  style={{ width: `${progress(r?.points ?? 0, r?.total_points ?? 0)}%` }}
                />
              </div>
            </dd>
            <dt className="text-ink-faint">复检点数</dt>
            <dd className="font-mono tabular-nums text-ink">
              {r ? `${r.verify_points}/${r.total_points}` : "—"}
            </dd>
            <dd className="col-start-2 pb-0.5">
              <div className="h-1.5 rounded bg-field">
                <div
                  className="h-full rounded bg-accent"
                  style={{ width: `${progress(r?.verify_points ?? 0, r?.total_points ?? 0)}%` }}
                />
              </div>
            </dd>
            <dt className="text-ink-faint">落盘</dt>
            <dd className="text-ink">{r?.saved ? "已落盘" : "未落盘"}</dd>
            <dt className="text-ink-faint">validMask</dt>
            <dd className="font-mono tabular-nums text-ink">
              {r ? `0x${r.valid_mask.toString(16).toUpperCase()}` : "—"}
            </dd>
            <dt className="text-ink-faint">Flash 记录</dt>
            <dd className="text-ink">{r?.record_loaded ? "已加载" : "无"}</dd>
          </dl>

          <div className="mt-3">
            <div className="mb-1 text-xs text-ink-faint">通道系数（c0..c3）</div>
            <table className="w-full text-[13px]">
              <tbody>
                {(r?.channels ?? []).map((ch) => (
                  <tr key={ch.ch}>
                    <td className="w-12 py-0.5 font-mono text-ink-soft">ch{ch.ch}</td>
                    <td className="py-0.5">
                      <Pill label={ch.valid ? "已校准" : "未校准"} tone={ch.valid ? "green" : "slate"} />
                    </td>
                    <td className="py-0.5 font-mono tabular-nums text-ink-soft">
                      {ch.coeff ? ch.coeff.map((c) => num(c, 6)).join(", ") : "—"}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </Card>

        <Card>
          <CardTitle
            right={
              ref && !ref.no_reply ? (
                <Pill label={ref.has ? "有校准记录" : "无校准记录"} tone={ref.has ? "green" : "amber"} />
              ) : undefined
            }
          >
            REF（参考板）
          </CardTitle>
          {!ref ? (
            <div className="text-xs text-ink-faint">未查询（连接后自动拉取）</div>
          ) : ref.no_reply ? (
            <div className="rounded-lg bg-danger-soft p-2 text-xs text-danger-ink">
              REF 无应答（检查 RS485 连线 / REF 是否上电）
            </div>
          ) : (
            <>
              <dl className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-1.5 text-[13px]">
                <dt className="text-ink-faint">板型</dt>
                <dd className="text-ink">{boardLabel(ref.board)}</dd>
                <dt className="text-ink-faint">版本</dt>
                <dd className="font-mono tabular-nums text-ink">{ref.version ?? "—"}</dd>
                <dt className="text-ink-faint">validMask</dt>
                <dd className="font-mono tabular-nums text-ink">0x{ref.valid_mask.toString(16).toUpperCase()}</dd>
              </dl>
              <div className="mt-3 flex flex-wrap gap-2">
                {[0, 1, 2, 3].map((ch) => {
                  const ok = (ref.valid_mask >> ch) & 1;
                  return <Pill key={ch} label={`ch${ch} ${ok ? "已校准" : "未校准"}`} tone={ok ? "green" : "slate"} />;
                })}
              </div>
            </>
          )}
        </Card>
      </div>

      <div className="flex flex-col gap-4">
        <CalibChart report={r} />
        <CalibResult report={r} />
      </div>
    </div>
  );
}
