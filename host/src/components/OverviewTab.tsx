import type { CalibReport, Status } from "../types";
import * as api from "../lib/ipc";
import { Button, Card, CardTitle, Pill, type Tone } from "./ui";

const num = (v: number | null | undefined, digits = 2) =>
  v === null || v === undefined ? "—" : v.toFixed(digits);

const stateTone = (state: string): Tone => {
  switch (state) {
    case "attached":
      return "green";
    case "searching":
      return "amber";
    case "error":
      return "red";
    case "connecting":
      return "blue";
    default:
      return "slate";
  }
};

const boardLabel = (dir: string | null | undefined) =>
  dir === "ad" ? "AD 采集板" : dir === "da" ? "DA 输出板" : "—";

export function OverviewTab({
  status,
  report,
  probeName,
}: {
  status: Status;
  report: CalibReport | null;
  probeName: string | null;
}) {
  const ref = report?.ref ?? null;

  return (
    <div className="flex flex-col gap-4">
      <Card>
        <div className="flex flex-wrap items-center gap-2">
          <Pill label={status.connected ? "已连接" : "未连接"} tone={status.connected ? "green" : "slate"} />
          <Pill label={`状态 ${status.state || "—"}`} tone={stateTone(status.state)} />
          {status.attempts > 0 && <Pill label={`重试 ${status.attempts}`} tone="amber" />}
          {probeName && <Pill label={probeName} tone="slate" />}
          <div className="ml-auto">
            <Button disabled={!status.connected} onClick={() => void api.calibStatus().catch(() => {})}>
              刷新信息
            </Button>
          </div>
        </div>
        {status.message && <div className="mt-2 text-xs text-slate-400">{status.message}</div>}
        {status.hint && (
          <div className="mt-2 select-all rounded-lg border border-amber-800 bg-amber-950/30 p-2 font-mono text-xs text-amber-300">
            {status.hint}
          </div>
        )}
        {status.state === "error" && (
          <div className="mt-2">
            <Button
              variant="primary"
              disabled={!status.connected}
              onClick={() => void api.resetTarget(false).catch(() => {})}
            >
              复位并运行
            </Button>
          </div>
        )}
      </Card>

      <div className="grid grid-cols-1 gap-4 xl:grid-cols-2">
        <Card>
          <CardTitle>DUT（本板）</CardTitle>
          <dl className="grid grid-cols-2 gap-y-1 text-xs">
            <dt className="text-slate-500">设备</dt>
            <dd className="font-mono text-slate-200">{report?.device ?? "—"}</dd>
            <dt className="text-slate-500">固件版本</dt>
            <dd className="font-mono text-slate-200">{report?.app_version ?? "—"}</dd>
            <dt className="text-slate-500">角色</dt>
            <dd className="font-mono text-slate-200">{report?.role ?? "—"}</dd>
            <dt className="text-slate-500">板型方向</dt>
            <dd className="text-slate-200">{boardLabel(report?.board_dir)}</dd>
            <dt className="text-slate-500">阶段</dt>
            <dd className="font-mono text-slate-200">{report?.phase ?? "—"}</dd>
            <dt className="text-slate-500">点数</dt>
            <dd className="font-mono text-slate-200">
              {report ? `${report.points}/${report.total_points}` : "—"}
            </dd>
            <dt className="text-slate-500">当前 run</dt>
            <dd className="font-mono text-slate-200">{report?.run_ch ?? "—"}</dd>
            <dt className="text-slate-500">落盘</dt>
            <dd className="text-slate-200">{report?.saved ? "已落盘" : "未落盘"}</dd>
            <dt className="text-slate-500">validMask</dt>
            <dd className="font-mono text-slate-200">
              {report ? `0x${report.valid_mask.toString(16).toUpperCase()}` : "—"}
            </dd>
            <dt className="text-slate-500">Flash 记录</dt>
            <dd className="text-slate-200">{report?.record_loaded ? "已加载" : "无"}</dd>
          </dl>

          <div className="mt-3">
            <div className="mb-1 text-xs text-slate-500">通道系数（c0..c3）</div>
            <table className="w-full border-collapse text-xs">
              <tbody>
                {(report?.channels ?? []).map((ch) => (
                  <tr key={ch.ch}>
                    <td className="w-12 py-0.5 font-mono text-slate-400">ch{ch.ch}</td>
                    <td className="py-0.5">
                      <Pill label={ch.valid ? "已校准" : "未校准"} tone={ch.valid ? "green" : "slate"} />
                    </td>
                    <td className="py-0.5 font-mono text-slate-300">
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
            <div className="text-xs text-slate-500">未查询（连接后自动拉取，或点「刷新信息」）</div>
          ) : ref.no_reply ? (
            <div className="rounded-lg border border-red-800 bg-red-950/30 p-2 text-xs text-red-300">
              REF 无应答（检查 RS485 连线 / REF 是否上电）
            </div>
          ) : (
            <>
              <dl className="grid grid-cols-2 gap-y-1 text-xs">
                <dt className="text-slate-500">板型</dt>
                <dd className="text-slate-200">{boardLabel(ref.board)}</dd>
                <dt className="text-slate-500">版本</dt>
                <dd className="font-mono text-slate-200">{ref.version ?? "—"}</dd>
                <dt className="text-slate-500">validMask</dt>
                <dd className="font-mono text-slate-200">0x{ref.valid_mask.toString(16).toUpperCase()}</dd>
              </dl>
              <div className="mt-3 flex flex-wrap gap-2">
                {[0, 1, 2, 3].map((ch) => {
                  const ok = (ref.valid_mask >> ch) & 1;
                  return (
                    <Pill key={ch} label={`ch${ch} ${ok ? "已校准" : "未校准"}`} tone={ok ? "green" : "slate"} />
                  );
                })}
              </div>
            </>
          )}
        </Card>
      </div>

      {report && report.errors.length > 0 && (
        <Card>
          <CardTitle>错误与警告 ({report.errors.length})</CardTitle>
          <ul className="max-h-40 space-y-0.5 overflow-auto font-mono text-xs text-red-300">
            {report.errors.map((e, i) => (
              <li key={i}>{e}</li>
            ))}
          </ul>
        </Card>
      )}
    </div>
  );
}
