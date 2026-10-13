import { useEffect, useState } from "react";
import { open } from "@tauri-apps/plugin-dialog";
import type { PipelineConfig, PipelineStatus, ProbeInfo } from "../types";
import * as api from "../lib/ipc";
import { Button, Card, CardTitle, Field, Input, Pill, Select, Switch } from "./ui";

interface Props {
  probes: ProbeInfo[];
  pipeline: PipelineStatus | null;
  onRefreshProbes: () => Promise<ProbeInfo[]>;
}

const CH_LABELS = ["ch0", "ch1", "ch2", "ch3"];
const STEPS = ["flash_dut", "attach_calib", "run_calib", "flash_app", "attach_verify"] as const;

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
      wait_next: "等待下一台",
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
  const [autoNext, setAutoNext] = useState(true);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    localStorage.setItem("prod.dutPath", dutPath);
  }, [dutPath]);
  useEffect(() => {
    localStorage.setItem("prod.appPath", appPath);
  }, [appPath]);

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
      auto_next: autoNext,
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

  const next = async () => {
    setBusy(true);
    setError(null);
    try {
      await api.nextPipeline();
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  };

  const isNextStep = pipeline?.step === "wait_next";
  const verifyPass = pipeline?.report?.verify_pass ?? null;

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
          <Field label="连续多台">
            <Switch checked={autoNext} disabled={disabled} onChange={setAutoNext} label="自动进入下一台" />
          </Field>
        </div>

        <div className="mt-3 flex items-center gap-2">
          <Button variant="primary" disabled={disabled} onClick={() => void start()}>
            开始流程
          </Button>
          <Button variant="danger" disabled={!running || busy} onClick={() => void stop()}>
            停止
          </Button>
          {isNextStep && (
            <Button variant="primary" onClick={() => void next()}>
              下一台
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
                  label={verifyPass === null ? "复检 进行中" : verifyPass ? "复检 PASS" : "复检 FAIL"}
                  tone={okTone(verifyPass)}
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
  const finished = step === "done" || step === "wait_next";
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
