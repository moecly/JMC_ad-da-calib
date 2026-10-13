import { useCallback, useEffect, useState } from "react";
import type { CalibParams } from "../types";
import * as api from "../lib/ipc";

const STORAGE_KEY = "host.calibParams";

export const DEFAULT_PARAMS: CalibParams = {
  residual_limit: 1000,
  check_gate: 16,
  stable_code: 20,
  settle_ms: 200,
  first_settle_ms: 600,
  sample_interval_ms: 5,
  ref_sample_interval_ms: 20,
  stable_count: 16,
  skip_verify: false,
};

function load(): CalibParams {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return DEFAULT_PARAMS;
    return { ...DEFAULT_PARAMS, ...(JSON.parse(raw) as Partial<CalibParams>) };
  } catch {
    return DEFAULT_PARAMS;
  }
}

export interface CalibParamsCtl {
  params: CalibParams;
  patch: (key: keyof CalibParams, value: number | boolean) => void;
  reset: () => void;
  apply: () => Promise<void>;
}

export function useCalibParams(): CalibParamsCtl {
  const [params, setParams] = useState<CalibParams>(load);

  useEffect(() => {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(params));
    } catch {
      // 本地存储不可用时静默回退为内存状态
    }
  }, [params]);

  const patch = useCallback((key: keyof CalibParams, value: number | boolean) => {
    setParams((prev) => ({ ...prev, [key]: value }));
  }, []);

  const reset = useCallback(() => setParams(DEFAULT_PARAMS), []);

  // 固件不持久化参数，复位回默认；每个读参数的指令前须重新下发
  const apply = useCallback(() => api.calibParams(params), [params]);

  return { params, patch, reset, apply };
}
