import { useEffect, useState } from "react";
import { open } from "@tauri-apps/plugin-dialog";
import type { CalibParams, PipelineConfig, PipelineStatus, ProbeInfo } from "../types";
import * as api from "../lib/ipc";
import { Button, Card, CardTitle, Field, Input, Pill, Select, Switch } from "./ui";

interface Props {
  probes: ProbeInfo[];
  pipeline: PipelineStatus | null;
  onRefreshProbes: () => Promise<ProbeInfo[]>;
}

const CH_LABELS = ["ch0", "ch1", "ch2", "ch3"];
const STEPS = ["flash_dut", "attach_calib", "run_calib", "flash_app", "attach_verify"] as const;

const DEFAULT_PARAMS: CalibParams = {
  residual_limit: 20,
  check_gate: 16,
  stable_code: 20,
  settle_ms: 200,
  first_settle_ms: 600,
  sample_interval_ms: 20,
  ref_sample_interval_ms: 20,
  stable_count: 16,
  skip_verify: false,
  retry: 3,
};

const NUM_FIELDS: Array<{ key: keyof CalibParams; label: string; min: number; max: number }> = [
  { key: "check_gate", label: "复检门限 (code)", min: 0, max: 65535 },
  { key: "residual_limit", label: "拟合残差门限 (code)", min: 0, max: 65535 },
  { key: "stable_code", label: "不稳定极差 (code)", min: 0, max: 65535 },
  { key: "settle_ms", label: "建压等待 (ms)", min: 1, max: 65535 },
  { key: "first_settle_ms", label: "首点建压 (ms)", min: 1, max: 65535 },
  { key: "sample_interval_ms", label: "AD 采样间隔 (ms)", min: 1, max: 65535 },
  { key: "ref_sample_interval_ms", label: "DA 采样间隔 (ms)", min: 1, max: 65535 },
  { key: "stable_count", label: "采样次数", min: 1, max: 32 },
  { key: "retry", label: "重试次数", min: 1, max: 9 },
];

function loadParams(): CalibParams {
  try {
    const raw = localStorage.getItem("prod.calibParams");
    return raw ? { ...DEFAULT_PARAMS, ...(JSON.parse(raw) as Partial<CalibParams>) } : DEFAULT_PARAMS;
  } catch {
    return DEFAULT_PARAMS;
  }
}

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

const okTone = (ok: boolean | null) =>
  ok === null ? "slate" : ok ? "green" : "red";

export function PipelinePanel({ probes, pipeline, onRefreshProbes }: Props) {
  const [probeIndex, setProbeIndex] = useState(0);
  const [chip, setChip] = useState("AT32F425K8T7");
  const [dutPath, setDutPath] = useState(() => localStorage.getItem("prod.dutPath") ?? "");
  const [appPath, setAppPath] = useState(() => localStorage.getItem("prod.appPath") ?? "");
  const [chMask, setChMask] = useState(0x0f);
  const [refDa, setRefDa] = useState<string[]>(["0", "1", "2", "3"]);
  const [params, setParams] = useState<CalibParams>(loadParams);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    localStorage.setItem("prod.dutPath", dutPath);
  }, [dutPath]);
  useEffect(() => {
    localStorage.setItem("prod.appPath", appPath);
  }, [appPath]);
  useEffect(() => {
    localStorage.setItem("prod.calibParams", JSON.stringify(params));
  }, [params]);

  const running = pipeline?.running ?? false;
  const disabled = busy || running;

  const pickFile = async (setter: (v: string) => void) => {
    const picked = await open({
      multiple: false,
      filters: [
        { name: "固件", extensions: ["elf", "hex"] },
        { name: "全部", extensions: ["*"] },
      ],
    });
    if (typeof picked === "string") setter(picked);
  };

  const start = async () => {
    if (!dutPath || !appPath) {
      setError("请选择校准固件与应用固件");
      return;
    }
    setBusy(true);
    setError(null);
    const cfg: PipelineConfig = {
      probe_index: probeIndex,
      chip,
      dut_path: dutPath,
      app_path: appPath,
      ch_mask: chMask,
      ref_da: refDa.map((s) => Number(s)),
      params,
    };
    try {
      await api.startPipeline(cfg);
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  };

  const stop = async () => {
    setBusy(true);
    setError(null);
    try {
      await api.stopPipeline();
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  };

  const isDone = !!pipeline && !pipeline.running && pipeline.step !== "idle";
  const verifyPass = pipeline?.report?.verify_pass ?? null;
  const skipped = pipeline?.report?.verify_skipped ?? false;

  return (
    <div className="flex flex-col gap-4">
      <Card>
        <CardTitle>流程配置</CardTitle>
        <div className="grid grid-cols-2 gap-3">
          <Field label="探针">
            <SelectProbe probes={probes} value={probeIndex} disabled={disabled} onChange={setProbeIndex} />
          </Field>
          <Field label="芯片">
            <Input value={chip} disabled={disabled} onChange={(e) => setChip(e.currentTarget.value)} />
          </Field>
          <Field label="校准固件 (DUT)">
            <div className="flex gap-2">
              <Input
                className="flex-1"
                value={dutPath}
                disabled={disabled}
                placeholder="选择 DUT 固件 ELF/HEX"
                onChange={(e) => setDutPath(e.currentTarget.value)}
              />
              <Button size="sm" disabled={disabled} onClick={() => void pickFile(setDutPath)}>
                浏览
              </Button>
            </div>
          </Field>
          <Field label="应用固件 (APP)">
            <div className="flex gap-2">
              <Input
                className="flex-1"
                value={appPath}
                disabled={disabled}
                placeholder="选择 APP 固件 ELF/HEX"
                onChange={(e) => setAppPath(e.currentTarget.value)}
              />
              <Button size="sm" disabled={disabled} onClick={() => void pickFile(setAppPath)}>
                浏览
              </Button>
            </div>
          </Field>
          <Field label="校准通道位掩码 (ch_mask)">
            <div className="flex flex-wrap items-center gap-1 pt-1">
              {CH_LABELS.map((ch, i) => (
                <label key={ch} className="flex items-center gap-1 rounded border border-line px-2 py-1 text-xs">
                  <input
                    type="checkbox"
                    checked={((chMask >> i) & 1) !== 0}
                    disabled={disabled}
                    onChange={() => setChMask((m) => m ^ (1 << i))}
                  />
                  {ch}
                </label>
              ))}
            </div>
          </Field>
          <Field label="REF DA 源 (每通道)" suffix="依次对应 ch0..ch3 的 REF DA 通道号">
            <div className="flex gap-2">
              {CH_LABELS.map((ch, i) => (
                <Input
                  key={ch}
                  className="w-16"
                  type="number"
                  value={refDa[i]}
                  disabled={disabled}
                  onChange={(e) => setRefDa((a) => a.map((v, j) => (j === i ? e.currentTarget.value : v)))}
                />
              ))}
            </div>
          </Field>
        </div>

        <div className="mt-4 border-t border-line pt-3">
          <div className="mb-2 flex flex-wrap items-center justify-between gap-2">
            <span className="text-xs font-medium text-ink-soft">校准参数（流程启动前下发，固件不持久化）</span>
            <Switch
              checked={params.skip_verify}
              disabled={disabled}
              onChange={(v) => setParams((p) => ({ ...p, skip_verify: v }))}
              label="跳过复检（以拟合残差为准）"
            />
          </div>
          <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
            {NUM_FIELDS.map(({ key, label, min, max }) => (
              <Field key={key} label={label}>
                <Input
                  type="number"
                  min={min}
                  max={max}
                  className="w-24"
                  disabled={disabled}
                  value={String(params[key])}
                  onChange={(e) => {
                    const n = Number(e.currentTarget.value);
                    if (!Number.isFinite(n)) return;
                    setParams((p) => ({ ...p, [key]: Math.max(min, Math.min(max, Math.round(n))) }));
                  }}
                />
              </Field>
            ))}
            <Field label=" ">
              <Button size="sm" disabled={disabled} onClick={() => setParams(DEFAULT_PARAMS)}>
                恢复默认
              </Button>
            </Field>
          </div>
        </div>

        <div className="mt-3 flex items-center gap-2">
          <Button variant="primary" disabled={disabled} onClick={() => void start()}>
            {isDone ? "重新运行" : "开始流程"}
          </Button>
          {running && (
            <Button variant="danger" disabled={busy} onClick={() => void stop()}>
              停止
            </Button>
          )}
          <Button size="sm" disabled={busy} onClick={() => void onRefreshProbes()}>
            刷新探针
          </Button>
          {error && <span className="text-xs text-danger-ink">{error}</span>}
        </div>
      </Card>

      <Card>
        <CardTitle
          right={
            <div className="flex items-center gap-2">
              {pipeline && (
                <Pill
                  label={
                    skipped ? "复检 已跳过" : verifyPass === null ? "复检 进行中" : verifyPass ? "复检 PASS" : "复检 FAIL"
                  }
                  tone={skipped ? "amber" : okTone(verifyPass)}
                />
              )}
              <Pill label={pipeline ? stepLabel(pipeline.step) : "待机"} tone={okTone(pipeline?.ok ?? null)} />
            </div>
          }
        >
          流程状态
        </CardTitle>
        {pipeline ? (
          <div className="flex flex-col gap-3 text-sm">
            <StepBar step={pipeline.step} ok={pipeline.ok} />
            <div className="text-ink-soft">{pipeline.message}</div>
          </div>
        ) : (
          <div className="text-sm text-ink-faint">尚未运行</div>
        )}
      </Card>
    </div>
  );
}

function StepBar({ step, ok }: { step: string; ok: boolean | null }) {
  const finished = step === "done";
  const cur = finished ? STEPS.length : (STEPS as readonly string[]).indexOf(step);
  return (
    <div className="flex items-center gap-2 rounded-lg bg-canvas p-3 text-xs">
      {STEPS.map((s, i) => (
        <div key={s} className={`flex items-center gap-2 ${i > 0 ? "flex-1" : ""}`}>
          {i > 0 && <span className="h-px flex-1 bg-line-strong" />}
          <span className="flex items-center gap-1.5 whitespace-nowrap">
            <span
              className={`inline-block h-2 w-2 rounded-full ${
                ok === false && i === cur
                  ? "bg-danger"
                  : i < cur
                    ? "bg-ok"
                    : i === cur
                      ? "bg-accent"
                      : "bg-line-strong"
              }`}
            />
            <span className={i <= cur ? "text-ink" : "text-ink-faint"}>{stepLabel(s)}</span>
          </span>
        </div>
      ))}
    </div>
  );
}

function SelectProbe({
  probes,
  value,
  disabled,
  onChange,
}: {
  probes: ProbeInfo[];
  value: number;
  disabled: boolean;
  onChange: (v: number) => void;
}) {
  return (
    <Select
      value={value}
      disabled={disabled}
      onChange={(e) => onChange(Number(e.currentTarget.value))}
    >
      {probes.length === 0 && <option value={0}>无探针</option>}
      {probes.map((p) => (
        <option key={p.index} value={p.index}>
          {p.name}
          {p.serial ? ` (${p.serial})` : ""}
        </option>
      ))}
    </Select>
  );
}
