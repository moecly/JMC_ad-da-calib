import type { CalibReport, CalibParams } from "../types";
import type { CalibParamsCtl } from "../hooks/useCalibParams";
import { Button, Card, CardTitle, Field, Input, Pill, Switch } from "./ui";

interface Props {
  connected: boolean;
  report: CalibReport | null;
  ctl: CalibParamsCtl;
}

const NUM_FIELDS: Array<{ key: keyof CalibParams; label: string; min: number; max: number }> = [
  { key: "check_gate", label: "复检门限 (code)", min: 0, max: 65535 },
  { key: "residual_limit", label: "拟合残差门限 (code)", min: 0, max: 65535 },
  { key: "stable_code", label: "不稳定极差 (code)", min: 0, max: 65535 },
  { key: "settle_ms", label: "建压等待 (ms)", min: 1, max: 65535 },
  { key: "first_settle_ms", label: "首点建压 (ms)", min: 1, max: 65535 },
  { key: "sample_interval_ms", label: "AD 采样间隔 (ms)", min: 1, max: 65535 },
  { key: "ref_sample_interval_ms", label: "DA 采样间隔 (ms)", min: 1, max: 65535 },
  { key: "stable_count", label: "采样次数", min: 1, max: 32 },
];

export function ParamsTab({ connected, report, ctl }: Props) {
  const { params, patch, reset, apply } = ctl;
  const fw = report?.fw_params ?? null;

  return (
    <div className="flex flex-col gap-4">
      <Card className="flex flex-col gap-4">
        <CardTitle right={<Pill label={connected ? "已连接" : "未连接"} tone={connected ? "green" : "slate"} />}>
          校准参数
        </CardTitle>

        <div className="text-xs text-ink-soft">
          参数由上位机在「配置 / 启动 / 重启 / 复检 / 手动写入」前自动下发，固件不持久化，复位回默认。
        </div>

        <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
          {NUM_FIELDS.map(({ key, label, min, max }) => (
            <Field key={key} label={label}>
              <Input
                type="number"
                min={min}
                max={max}
                className="w-28"
                disabled={!connected}
                value={String(params[key])}
                onChange={(e) => {
                  const n = Number(e.currentTarget.value);
                  if (!Number.isFinite(n)) return;
                  patch(key, Math.max(min, Math.min(max, Math.round(n))));
                }}
              />
            </Field>
          ))}
        </div>

        <Switch
          checked={params.skip_verify}
          disabled={!connected}
          onChange={(v) => patch("skip_verify", v)}
          label="校准后跳过复检（以拟合残差为准）"
        />

        <div className="flex flex-wrap items-center gap-2">
          <Button variant="primary" size="sm" disabled={!connected} onClick={() => void apply().catch(() => {})}>
            应用参数
          </Button>
          <Button size="sm" disabled={!connected} onClick={reset}>
            恢复默认
          </Button>
        </div>
      </Card>

      <Card>
        <CardTitle>设备已应用</CardTitle>
        {fw ? (
          <div className="flex flex-wrap gap-x-6 gap-y-1 font-mono text-[13px] tabular-nums text-ink-soft">
            <span>门限 {fw.gate}</span>
            <span>残差 {fw.resid}</span>
            <span>极差 {fw.stable}</span>
            <span>建压 {fw.settle} ms</span>
            <span>首点 {fw.first} ms</span>
            <span>AD 间隔 {fw.samp} ms</span>
            <span>DA 间隔 {fw.refsamp} ms</span>
            <span>采样 {fw.n}</span>
            <span>复检 {fw.skipv ? "跳过" : "开启"}</span>
          </div>
        ) : (
          <div className="text-ink-faint">尚无回执（下发参数后固件打印 `calib: params …`）</div>
        )}
      </Card>
    </div>
  );
}
